#pragma once
#include <cmath>
#include <cstdint>
#include <cstring>
namespace hammer_combat {
constexpr uint64_t AVATAR=UINT64_C(0x4D1C334D294DFA97),PRIMARY=UINT64_C(0xD06F08C204B2C362);
constexpr float STUN_RADIUS=6.0f;
inline bool primary_actor(uint64_t actor,uint32_t life,int32_t hp,uint64_t held,uint32_t selection) {
 return actor==AVATAR && !life && hp>0 && held==PRIMARY && selection==1;
}
inline bool in_stun_area(const float* blast,const float* target) {
 double d=0;
 for(unsigned i=0;i<3;i++) {
  if(!std::isfinite(blast[i])||!std::isfinite(target[i])||std::fabs(blast[i])>100000||std::fabs(target[i])>100000)return false;
  double delta=double(blast[i])-target[i];d+=delta*delta;
 }return d<=double(STUN_RADIUS)*STUN_RADIUS;
}
inline bool owned_byte_transition(const uint8_t* value,const uint8_t* before,const uint8_t* after,size_t n) {
 for(size_t i=0;i<n;i++)if(value[i]!=before[i]&&value[i]!=after[i])return false;return true;
}
}
