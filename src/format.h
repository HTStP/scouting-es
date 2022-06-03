#ifndef FORMAT_H
#define FORMAT_H

#include <stdint.h>
#include <math.h>

struct blockCalo{
  uint32_t calo0[8];
  uint32_t calo1[8];
  uint32_t calo2[8];
  uint32_t calo3[8];
  uint32_t calo4[8];
  uint32_t calo5[8];
};

struct block1{
  uint32_t orbit[8];
  uint32_t bx[8];
  uint32_t mu1f[8];
  uint32_t mu1s[8];
  uint32_t mu2f[8];
  uint32_t mu2s[8];
};

struct bx_map_frame{
uint32_t bx_map_l[8];
};

struct orbit_trailer{
uint32_t beefdead[8];
uint32_t bx_map[14*8];
uint32_t trailer1[8];
uint32_t trailer2[8];
};


struct muon{
  uint32_t f; //first word
  uint32_t s; //second word
  uint32_t extra; //for intermediate marker and eta
};

struct block{
  uint32_t bx;
  uint32_t orbit;
  muon mu[16];
};

//original format
/*struct masks{
  //bx word: 16 bits used for actual bx, MS 4 bits are muon type 
  //(0xf intermediate, 0x0 final, following 4 bits for link id)
  static constexpr  uint32_t bx      = 0xffff;
  static constexpr  uint32_t interm  = 0x000f;
  static constexpr  uint32_t linkid  = 0x000f;
  //masks for muon 64 bits
  static constexpr  uint32_t phiext  = 0x03ff;
  static constexpr  uint32_t pt      = 0x01ff;
  static constexpr  uint32_t qual    = 0x000f;
  static constexpr  uint32_t etaext  = 0x01ff;
  static constexpr  uint32_t etaextv = 0x00ff;
  static constexpr  uint32_t etaexts = 0x0100;
  static constexpr  uint32_t iso     = 0x0003;
  static constexpr  uint32_t chrg    = 0x0001;
  static constexpr  uint32_t chrgv   = 0x0001;
  static constexpr  uint32_t index   = 0x007f;
  static constexpr  uint32_t phi     = 0x03ff;
  static constexpr  uint32_t eta     = 0x01ff;
  static constexpr  uint32_t etav    = 0x00ff;
  static constexpr  uint32_t etas    = 0x0100;
  //NOTA BENE: reserved two bits are used for muon id
  //0x1==intermediate, 0x2==final
  static constexpr  uint32_t rsv     = 0x0003;
};*/

//run3 format --tj
struct masks{
  //bx word: 16 bits used for actual bx, MS 4 bits are muon type 
  //(0xf intermediate, 0x0 final, following 4 bits for link id)
  static constexpr  uint32_t bx      = 0x1fff;
  static constexpr  uint32_t interm  = 0x0001;
  //masks for muon 64 bits
  static constexpr  uint32_t phiext  = 0x03ff;
  static constexpr  uint32_t pt      = 0x01ff;
  static constexpr  uint32_t ptuncon = 0x00ff; //--8bits
  static constexpr  uint32_t qual    = 0x000f;
  static constexpr  uint32_t etaext  = 0x01ff;
  static constexpr  uint32_t etaextv = 0x00ff;
  static constexpr  uint32_t etaexts = 0x0100;
  static constexpr  uint32_t iso     = 0x0003;
  static constexpr  uint32_t chrg    = 0x0001;
  static constexpr  uint32_t chrgv   = 0x0001;
  static constexpr  uint32_t index   = 0x007f;
  static constexpr  uint32_t phi     = 0x03ff;
  static constexpr  uint32_t eta     = 0x01ff;
  static constexpr  uint32_t etav    = 0x00ff;
  static constexpr  uint32_t etas    = 0x0100;
  static constexpr  uint32_t impact  = 0x0003;
  
  //NOTA BENE: reserved two bits are used for muon id
  //0x1==intermediate, 0x2==final
//  static constexpr  uint32_t rsv     = 0x0003;//-- no longer anything reserved
};

struct shifts{
  //bx word: 16 bits used for actual bx, MS 4 bits are muon type 
  //(0xf intermediate, 0x0 final, following 4 bits for link id)
  static constexpr  uint32_t bx      = 0;
  static constexpr  uint32_t interm  = 31; //updated for new run3 format //tj
  //static constexpr  uint32_t linkid  = 23; //no longer exists in data format //tj
  //shifts for muon 64 bits
  static constexpr  uint32_t  phiext =  0;
  static constexpr  uint32_t  pt     = 10;
  static constexpr  uint32_t  qual   = 19;
  static constexpr  uint32_t  etaext = 23;
  static constexpr  uint32_t  iso    =  0;
  static constexpr  uint32_t  chrg   =  2;
  static constexpr  uint32_t  chrgv  =  3;
  static constexpr  uint32_t  index  =  4;
  static constexpr  uint32_t  phi    = 11;
  //static constexpr  uint32_t  eta    = 21; --hack for now --tj -- only store etaext
  static constexpr  uint32_t  ptuncon = 21;
  static constexpr  uint32_t  impact = 30;
};

struct header_shifts{
  static constexpr uint32_t bxmatch    = 24;
  static constexpr uint32_t mAcount    = 16;
  static constexpr uint32_t orbitmatch =  8;
  static constexpr uint32_t mBcount    =  0;
};


struct header_masks{
  static constexpr uint32_t bxmatch    = 0xff << header_shifts::bxmatch;
  static constexpr uint32_t mAcount    = 0x0f << header_shifts::mAcount;
  static constexpr uint32_t orbitmatch = 0xff << header_shifts::orbitmatch;
  static constexpr uint32_t mBcount    = 0x0f << header_shifts::mBcount;
};

struct gmt_scales{
  static constexpr float pt_scale  = 0.5;
  static constexpr float phi_scale = 2.*M_PI/576.;
  static constexpr float eta_scale = 0.0870/8; //9th MS bit is sign
  static constexpr float phi_range = M_PI;
};

struct constants{
  static constexpr uint32_t deadbeef           = 0xdeadbeef;
  static constexpr uint32_t beefdead           = 0xbeefdead;
  static constexpr uint32_t intermediate_marker= 0x0000000f;
  static constexpr uint32_t orbit_trailer_size = 544;
  static constexpr uint32_t intermediate       = 0x00000001;
  static constexpr uint32_t final              = 0x00000001;
};


#endif
