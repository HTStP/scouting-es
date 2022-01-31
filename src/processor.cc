#include "processor.h"
#include "format.h"
#include "slice.h"
#include "log.h"
#include <iomanip>

StreamProcessor::StreamProcessor(size_t max_size_, bool doZS_, std::string systemName_) : 
	tbb::filter(parallel),
	max_size(max_size_),
	nbPackets(0),
	doZS(doZS_),
	systemName(systemName_)
{ 
	LOG(TRACE) << "Created transform filter at " << static_cast<void*>(this);
	myfile.open ("example.txt");
}  


StreamProcessor::~StreamProcessor(){
	//  fprintf(stderr,"Wrote %d muons \n",totcount);
	myfile.close();
}

Slice* StreamProcessor::process(Slice& input, Slice& out)
{
	nbPackets++;
	char* p = input.begin();
	char* q = out.begin();
	uint32_t counts = 0;
	
	if(systemName =="DMX"){
        memcpy(q,p,input.size());
	out.set_end(out.begin() + input.size());
	out.set_counts(1);
	return &out;}
	
	int bsize = sizeof(block1);
	if((input.size()-constants::orbit_trailer_size)%bsize!=0){
		LOG(WARNING)
			<< "Frame size not a multiple of block size. Will be skipped. Size="
			<< input.size() << " - block size=" << bsize;
		return &out;
	}


	while(p!=input.end()){
		bool brill_word = false;
		bool endoforbit = false;
		block1 *bl = (block1*)p;
		int mAcount = 0;
		int mBcount = 0;
		uint32_t bxmatch=0;
		uint32_t orbitmatch=0;
		uint32_t brill_marker = 0xFF;
		bool AblocksOn[8];
		bool BblocksOn[8];
		for(unsigned int i = 0; i < 8; i++){
			if(bl->orbit[i]==constants::deadbeef){
				p += constants::orbit_trailer_size;
				endoforbit = true;
				break;
			}
			bool brill_enabled = 0;

			if(( brill_enabled) && ((bl->orbit[i] == 0xFF) ||( bl->bx[i] == 0xFF) ||( bl->mu1f[i] == 0xFF) || 
					(bl->mu1s[i] == 0xFF) ||( bl->mu2f[i] == 0xFF) ||( bl->mu2s[i] == 0xFF))){
				brill_word = true;
			}

			//	std::cout << bl->orbit[i] << std::endl;
			/*			if (bl->orbit[i] > 258745337){
						std::cout << bl->orbit[i] << std::endl;
						brill_word = true;			
						std::cout << "orbit " << bl->orbit[i] << std::endl;
						std::cout << "bx " << bl->bx[i] << std::endl;
						}
						*/
			uint32_t bx = (bl->bx[i] >> shifts::bx) & masks::bx;
			uint32_t interm = (bl->bx[i] >> shifts::interm) & masks::interm;
			uint32_t orbit = bl->orbit[i];

			bxmatch += (bx==((bl->bx[0] >> shifts::bx) & masks::bx))<<i;
			orbitmatch += (orbit==bl->orbit[0])<<i; 
			uint32_t pt = (bl->mu1f[i] >> shifts::pt) & masks::pt;
			uint32_t etae = (bl->mu1f[i] >> shifts::etaext) & masks::eta;
			//			std::cout << bx << "," << orbit << "," << interm << "," << etae << std::endl;

			AblocksOn[i]=((pt>0) || (doZS==0) || (brill_word));
			if((pt>0) || (doZS==0) || (brill_word)){
				mAcount++;
				//std::cout << "mAcount +" << std::endl;
			}
			pt = (bl->mu2f[i] >> shifts::pt) & masks::pt;
			BblocksOn[i]=((pt>0) || (doZS==0) ||( brill_word));
			if((pt>0) || (doZS==0) || (brill_word)){
				mBcount++;
				//std::cout << "mBcount +" << std::endl;
			}
		}
		if(endoforbit) continue;
		uint32_t bxcount = std::max(mAcount,mBcount);
		if(bxcount == 0) {
			p+=bsize;
			LOG(WARNING) << '#' << nbPackets << ": Detected a bx with zero muons, this should not happen. Packet is skipped."; 
			continue;
		}

		uint32_t header = (bxmatch<<24)+(mAcount << 16) + (orbitmatch<<8) + mBcount;

		counts += mAcount;
		counts += mBcount;
		memcpy(q,(char*)&header,4); q+=4;
		memcpy(q,(char*)&bl->bx[0],4); q+=4;
		memcpy(q,(char*)&bl->orbit[0],4); q+=4;
		for(unsigned int i = 0; i < 8; i++){
			if(AblocksOn[i]){
				memcpy(q,(char*)&bl->mu1f[i],4); q+=4;
				memcpy(q,(char*)&bl->mu1s[i],4); q+=4;
				//memcpy(q,(char*)&(bl->bx[i] &= ~0x1),4); q+=4; //set bit 0 to 0 for first muon
		// next creating mu.extra which is a copy of bl->bx with a change to the first bit		
				if(brill_word == true){
					memcpy(q,&brill_marker,4); q+=4;
				}	else {
					memcpy(q,(char*)&(bl->bx[i] &= ~0x1),4); q+=4; //set bit 0 to 0 for first muon
				}
			}
		}

		for(unsigned int i = 0; i < 8; i++){
			if(BblocksOn[i]){
				memcpy(q,(char*)&bl->mu2f[i],4); q+=4;
				memcpy(q,(char*)&bl->mu2s[i],4); q+=4;
				//memcpy(q,(char*)&(bl->bx[i] |= 0x1),4); q+=4; //set bit 0 to 1 for second muon
		// next creating mu.extra which is a copy of bl->bx with a change to the first bit		
		if(brill_word == true){
					memcpy(q,&brill_marker,4); q+=4; 
				}	else{
					memcpy(q,(char*)&(bl->bx[i] |= 0x1),4); q+=4; //set bit 0 to 1 for second muon
				}							
			}
		}

		p+=sizeof(block1);

	}


	
	out.set_end(q);
	out.set_counts(counts);
	return &out;  
}

void* StreamProcessor::operator()( void* item ){
	Slice& input = *static_cast<Slice*>(item);
	Slice& out = *Slice::allocate( 2*max_size);
	process(input, out);
	Slice::giveAllocated(&input);

	return &out;
}
