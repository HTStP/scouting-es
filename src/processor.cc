#include "processor.h"
#include "format.h"
#include "slice.h"
#include "log.h"
#include <iomanip>
#include <cmath>
#include <vector>
#include <algorithm>

StreamProcessor::StreamProcessor(size_t max_size_, bool doZS_, ProcessorType processorType_) :
	tbb::filter(parallel),
	max_size(max_size_),
	nbPackets(0),
	doZS(doZS_),
	processorType(processorType_)
{ 
	LOG(TRACE) << "Created transform filter at " << static_cast<void*>(this);
	myfile.open ("example.txt");
}  


std::vector<uint32_t> bit_check(uint32_t word, uint32_t offset)
{
	std::vector<uint32_t> vect;
	for (uint32_t i = 0; i<32; i++)
	{
		if (word & 1){
			vect.push_back(i + offset);
		}
		word >>= 1;
	}

	return vect;
}

StreamProcessor::~StreamProcessor(){
	//  fprintf(stderr,"Wrote %d muons \n",totcount);
	myfile.close();
}

bool StreamProcessor::CheckFrameMultBlock(uint32_t inputSize){

	int bsize = sizeof(block1);
	if((inputSize-20*constants::orbit_trailer_size - 32*20 -32)%bsize!=0){
		LOG(WARNING)
			<< "Frame size not a multiple of block size. Will be skipped. Frame size = "
			<< inputSize << ", block size = " << bsize;
		return false;
	}
	return true;
}

std::vector<unsigned int> StreamProcessor::CountBX(Slice& input){

	char* p = input.begin() + 32; // +32 to account for orbit header
	std::vector<unsigned int> bx_vect;
	while( p != input.end()){
		block1 *bl = (block1*)(p);
		if(bl->orbit[0]==constants::beefdead){ // found orbit trailer
			orbit_trailer *ot = (orbit_trailer*)(p);
			for (unsigned int k = 0; k < (14*8); k++){ // 14*8 = 14 frames, 8 links of orbit trailer containing BX hitmap
				//bxcount += __builtin_popcount(ot->bx_map[k]);
				std::vector<unsigned int> bx_vect_t = bit_check(ot->bx_map[k], k*32);
				bx_vect.insert(bx_vect.end(), bx_vect_t.begin(), bx_vect_t.end());
			}
			return bx_vect;
		}

		p+=sizeof(block1);
	}

}

uint32_t StreamProcessor::FillOrbit(Slice& input, Slice& out, std::vector<unsigned int>* bx_vect){
	char* p = input.begin() + 32; // +32 to account for orbit header
	char* q = out.begin(); // +32 to account for orbit header
	uint32_t relbx = 0;
	uint32_t counts = 0;
	std::cout << "here1 "<< std::endl;
	while(relbx < bx_vect->size()){
		block1 *bl = (block1*)(p);
		if(bl->orbit[0]==constants::beefdead){break;} 
		int mAcount = 0;
		int mBcount = 0;
		uint32_t bxmatch=0;
		uint32_t orbitmatch=0;
		bool AblocksOn[8];
		bool BblocksOn[8];
		for(unsigned int i = 0; i < 8; i++){
			uint32_t bxA = (bl->bx[i] >> shifts::bx) & masks::bx;
			uint32_t bx = bx_vect->at(relbx);
			//std::cout << "bxA = " << std::dec << bxA << ", bx = "<< bx << std::endl;
			uint32_t interm = (bl->bx[i] >> shifts::interm) & masks::interm;
			uint32_t orbit = bl->orbit[i];

			bxmatch += (bx==bx_vect->at(relbx))<<i;
			orbitmatch += (orbit==bl->orbit[0])<<i; 
			uint32_t pt = (bl->mu1f[i] >> shifts::pt) & masks::pt;
			uint32_t etae = (bl->mu1f[i] >> shifts::etaext) & masks::eta;

			AblocksOn[i]=((pt>0) || (doZS==0));
			if((pt>0) || (doZS==0)){
				mAcount++;
			}
			pt = (bl->mu2f[i] >> shifts::pt) & masks::pt;
			BblocksOn[i]=((pt>0) || (doZS==0));
			if((pt>0) || (doZS==0)){
				mBcount++;
			}

		}
		uint32_t bxcount = std::max(mAcount,mBcount);
		if(bxcount == 0) {
			p+=sizeof(block1);
			LOG(WARNING) << '#' << nbPackets << ": Detected a bx with zero muons, this should not happen. Packet is skipped."; 
			continue;
		}

		uint32_t header = (bxmatch<<24)+(mAcount << 16) + (orbitmatch<<8) + mBcount;

		counts += mAcount;
		counts += mBcount;
		memcpy(q,(char*)&header,4); q+=4;
		memcpy(q,(char*)&bx_vect->at(relbx),4); q+=4;
		memcpy(q,(char*)&bl->orbit[0],4); q+=4;
		for(unsigned int i = 0; i < 8; i++){
			if(AblocksOn[i]){
				memcpy(q,(char*)&bl->mu1f[i],4); q+=4;
				memcpy(q,(char*)&bl->mu1s[i],4); q+=4;
				// next creating mu.extra which is a copy of bl->bx with a change to the first bit		
				memcpy(q,(char*)&(bx_vect->at(relbx) &= ~0x1),4); q+=4; //set bit 0 to 0 for first muon
			}
		}

		for(unsigned int i = 0; i < 8; i++){
			if(BblocksOn[i]){
				memcpy(q,(char*)&bl->mu2f[i],4); q+=4;
				memcpy(q,(char*)&bl->mu2s[i],4); q+=4;
				// next creating mu.extra which is a copy of bl->bx with a change to the first bit		
				memcpy(q,(char*)&(bx_vect->at(relbx) |= 0x1),4); q+=4; //set bit 0 to 1 for second muon
			}
		}

		p+=sizeof(block1);

		relbx++;
	}
	return counts;
}

Slice* StreamProcessor::process(Slice& input, Slice& out)
{
	nbPackets++;
	char* p = input.begin(); 
	char* q = out.begin();
	uint32_t counts = 0;
	bool endofpacket = false;

	if (processorType == ProcessorType::PASS_THROUGH) {
		memcpy(q,p,input.size());
		out.set_end(out.begin() + input.size());
		out.set_counts(1);
		return &out;
	}

	if (!CheckFrameMultBlock(input.size())){ return &out; } 
	while (endofpacket == false){

		std::vector<unsigned int> bx_vect = CountBX(input);
		std::sort(bx_vect.begin(), bx_vect.end());
		uint32_t orbitCount = FillOrbit(input, out, &bx_vect);
		p+= 32 + bx_vect.size()*sizeof(block1) + constants::orbit_trailer_size; // 32 for orbit header, + nBXs + orbit trailer
		q+= orbitCount*12 + 12*bx_vect.size(); // 12 bytes for each muon/count then 12 bytes for each bx header
		counts += orbitCount;
		bx_vect.clear();


		if(p < input.end()){

			uint32_t *dma_trailer_word = (uint32_t*)(p);
			if( *dma_trailer_word == constants::deadbeef){
				endofpacket = true;
				out.set_end(q);
				std::cout << "counts " << counts << std::endl;
				out.set_counts(counts);
				return &out;
			}
		}
	}
}
void* StreamProcessor::operator()( void* item ){
	Slice& input = *static_cast<Slice*>(item);
	Slice& out = *Slice::allocate( 2*max_size);
	process(input, out);
	Slice::giveAllocated(&input);

	return &out;
}
