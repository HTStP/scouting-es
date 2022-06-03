#include "processor.h"
#include "format.h"
#include "slice.h"
#include "log.h"
#include <iomanip>
#include <cmath>
#include <vector>
#include <algorithm>

StreamProcessor::StreamProcessor(size_t max_size_, bool doZS_, ProcessorType processorType_, uint32_t nOrbitsPerDMAPacket_) :
	tbb::filter(parallel),
	max_size(max_size_),
	nbPackets(0),
	doZS(doZS_),
	processorType(processorType_),
	nOrbitsPerDMAPacket(nOrbitsPerDMAPacket_)
{ 
	LOG(TRACE) << "Created transform filter at " << static_cast<void*>(this);
	myfile.open ("example.txt");
}  

// Loops over each word in the orbit trailer BX map and fills a vector with the non-empty BX values
void bit_check(std::vector<unsigned int>* bx_vect, uint32_t word, uint32_t offset)
{
	for (uint32_t i = 0; i<32; i++)
	{
		if (word & 1){
			bx_vect->push_back(i + offset);
		}
		word >>= 1;
	}
	return;
}

StreamProcessor::~StreamProcessor(){
	//  fprintf(stderr,"Wrote %d muons \n",totcount);
	myfile.close();
}

// checks that the packet size is an integer multiple of the BX block size, minus the header/trailers
bool StreamProcessor::CheckFrameMultBlock(uint32_t inputSize){

	int bsize = sizeof(block1);
	if((inputSize-nOrbitsPerDMAPacket*constants::orbit_trailer_size - 32*nOrbitsPerDMAPacket -32)%bsize!=0){
		LOG(WARNING)
			<< "Frame size not a multiple of block size. Will be skipped. Frame size = "
			<< inputSize << ", block size = " << bsize;
		return false;
	}
	return true;
}

// Looks for orbit trailer then counts the non-empty bunch crossings and fills a vector with their values
std::vector<unsigned int> StreamProcessor::CountBX(Slice& input){

	char* p = input.begin() + 32; // +32 to account for orbit header
	std::vector<unsigned int> bx_vect;
	while( p != input.end()){
		block1 *bl = (block1*)(p);
		if(bl->orbit[0]==constants::beefdead){ // found orbit trailer
			orbit_trailer *ot = (orbit_trailer*)(p);
			for (unsigned int k = 0; k < (14*8); k++){ // 14*8 = 14 frames, 8 links of orbit trailer containing BX hitmap
				//bxcount += __builtin_popcount(ot->bx_map[k]);
				bit_check(&bx_vect, ot->bx_map[k], k*32);
			}
			return bx_vect;
		}

		p+=sizeof(block1);
	}

}

// Goes through orbit worth of data and fills the output memory with the calo data corresponding to the non-empty bunchcrossings, as marked in bx_vect
uint32_t StreamProcessor::FillOrbitCalo(Slice& input, Slice& out, std::vector<unsigned int>& bx_vect){
	//get orbit from orbit header
	char* s = input.begin();
	blockCalo *bl_pre = (blockCalo*)(s);
	uint32_t orbitN = bl_pre->calo0[0];
	
	//save warning_test_enable bit
	bool warning_test_enable = ( orbitN >> 0) & 1U;

	//remove warning_test_enable bit from orbit header
	orbitN |= 1UL << 0;	
	
	char* p = input.begin() + 32; // +32 to account for orbit header
	char* q = out.begin();
	uint32_t relbx = 0;
	uint32_t counts = 0;
	while(relbx < bx_vect.size()){ //total number of non-empty BXs in orbit is given by bx_vect.size()
		blockCalo *bl = (blockCalo*)(p);
		if(bl->calo0[0]==constants::beefdead){break;} // orbit trailer has been reached, end of orbit data

		uint32_t header = (uint32_t)warning_test_enable; //header can be added to later
		memcpy(q,(char*)&header,4); q+=4;
		memcpy(q,(char*)&bx_vect[relbx],4); q+=4;
		memcpy(q,(char*)&orbitN,4); q+=4;
		for(uint32_t i = 0; i < 8; i++){
			memcpy(q,(char*)&i,4); q+=4; //gives link number, can later be used to find object type
			memcpy(q,(char*)&bl->calo0[i],4); q+=4;
			memcpy(q,(char*)&bl->calo1[i],4); q+=4;
			memcpy(q,(char*)&bl->calo2[i],4); q+=4;
			memcpy(q,(char*)&bl->calo3[i],4); q+=4;
			memcpy(q,(char*)&bl->calo4[i],4); q+=4;
			memcpy(q,(char*)&bl->calo5[i],4); q+=4;
		}

		counts += 1;
		p+=sizeof(blockCalo);
		relbx++;
		}

return counts;
}

// Goes through orbit worth of data and fills the output memory with the muons corresponding to the non-empty bunchcrossings, as marked in bx_vect
uint32_t StreamProcessor::FillOrbitMuon(Slice& input, Slice& out, std::vector<unsigned int>& bx_vect){
	//get orbit from orbit header
	char* s = input.begin();
	block1 *bl_pre = (block1*)(s);
	uint32_t orbitN = bl_pre->orbit[0];
	
	char* p = input.begin() + 32; // +32 to account for orbit header
	char* q = out.begin(); // +32 to account for orbit header
	uint32_t relbx = 0;
	uint32_t counts = 0;
	while(relbx < bx_vect.size()){ //total number of non-empty BXs in orbit is given by bx_vect.size()
		block1 *bl = (block1*)(p);
		if(bl->orbit[0]==constants::beefdead){break;} // orbit trailer has been reached, end of orbit data
		int mAcount = 0;
		int mBcount = 0;
		uint32_t bxmatch=0;
		uint32_t orbitmatch=0;
		bool AblocksOn[8];
		bool BblocksOn[8];
		for(unsigned int i = 0; i < 8; i++){
			uint32_t bxA = (bl->bx[i] >> shifts::bx) & masks::bx;
			uint32_t bx = bx_vect[relbx];
			//std::cout << "bxA = " << std::dec << bxA << ", bx = "<< bx << std::endl;
			uint32_t interm = (bl->bx[i] >> shifts::interm) & masks::interm;
			//uint32_t orbit = bl->orbit[i];
			uint32_t orbit = orbitN;;

			bxmatch += (bx==bx_vect[relbx])<<i;
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
		memcpy(q,(char*)&bx_vect[relbx],4); q+=4;
		memcpy(q,(char*)&orbitN,4); q+=4;
		for(unsigned int i = 0; i < 8; i++){
			if(AblocksOn[i]){
				memcpy(q,(char*)&bl->mu1f[i],4); q+=4;
				memcpy(q,(char*)&bl->mu1s[i],4); q+=4;
				// next creating mu.extra which is a copy of bl->bx with a change to the first bit		
				memcpy(q,(char*)&(bx_vect[relbx] &= ~0x1),4); q+=4; //set bit 0 to 0 for first muon
			}
		}

		for(unsigned int i = 0; i < 8; i++){
			if(BblocksOn[i]){
				memcpy(q,(char*)&bl->mu2f[i],4); q+=4;
				memcpy(q,(char*)&bl->mu2s[i],4); q+=4;
				// next creating mu.extra which is a copy of bl->bx with a change to the first bit		
				memcpy(q,(char*)&(bx_vect[relbx] |= 0x1),4); q+=4; //set bit 0 to 1 for second muon
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
		uint32_t orbitCount;
		std::vector<unsigned int> bx_vect = CountBX(input);
		std::sort(bx_vect.begin(), bx_vect.end());
		if (processorType == ProcessorType::GMT) {
			orbitCount = FillOrbitMuon(input, out, bx_vect);
		}else if (processorType == ProcessorType::CALO){
			orbitCount = FillOrbitCalo(input, out, bx_vect);				
		}else{

                        LOG(ERROR) << "UNKNOWN PROCESSOR_TYPE, EXITING";
    			throw std::invalid_argument("ERROR: PROCESSOR_TYPE NOT RECOGNISED");
		}

		p+= 32 + bx_vect.size()*sizeof(block1) + constants::orbit_trailer_size; // 32 for orbit header, + nBXs + orbit trailer
		q+= orbitCount*12 + 12*bx_vect.size(); // 12 bytes for each muon/count then 12 bytes for each bx header
		counts += orbitCount;
		bx_vect.clear();


		if(p < input.end()){

			uint32_t *dma_trailer_word = (uint32_t*)(p);
			if( *dma_trailer_word == constants::deadbeef){
				endofpacket = true;
				out.set_end(q);
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
