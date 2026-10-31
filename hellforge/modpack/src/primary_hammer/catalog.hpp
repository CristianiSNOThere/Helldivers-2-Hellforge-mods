#pragma once
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <vector>
namespace hammer {
using Bytes=std::vector<uint8_t>;
inline void require(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
template<class T> T get(const Bytes& b,size_t at){require(at<=b.size() && sizeof(T)<=b.size()-at,"field outside record");T v;std::memcpy(&v,b.data()+at,sizeof v);return v;}
template<class T> void set(Bytes& b,size_t at,T value){require(at<=b.size() && sizeof(T)<=b.size()-at,"field outside record");std::memcpy(b.data()+at,&value,sizeof value);}
template<class T> Bytes scalar(T value){Bytes b(sizeof value);std::memcpy(b.data(),&value,sizeof value);return b;}
// Catalogue DTO layout was previously established by the workspace's Sickle adapter.
// Clone an uncustomizable root; native catalogue finalization owns all derived indexes.
inline Bytes catalogue(const Bytes& input,uint32_t count,uint32_t item){
 require(count>0 && count<4096 && input.size()==size_t(count)*112,"catalogue bounds");
 size_t donor=SIZE_MAX;
 for(uint32_t i=0;i<count;i++){
  size_t at=size_t(i)*112;
  require(get<uint32_t>(input,at)!=item && get<uint32_t>(input,at+24)!=item,"new item ID collision");
  require(get<uint32_t>(input,at+8)!=0x98df943d,"MA5C unexpectedly has children");
  if(get<uint32_t>(input,at+24)==0x98df943d){
   require(donor==SIZE_MAX && get<uint32_t>(input,at)==0x98df943d && !get<uint32_t>(input,at+8) && get<uint32_t>(input,at+36)==1,"MA5C root shape");donor=at;
  }
 }
 require(donor!=SIZE_MAX,"MA5C root absent");
 Bytes out=input;out.insert(out.end(),input.begin()+donor,input.begin()+donor+112);
 size_t at=input.size();set<uint32_t>(out,at,item);set<uint32_t>(out,at+24,item);
 // Free authored entry; borrowed donor arrays remain owned by the parser.
 // Ownership is separately gated on the original support-hammer purchase.
 std::memset(out.data()+at+56,0,12);std::memset(out.data()+at+72,0,12);std::memset(out.data()+at+88,0,12);
 return out;
}
}
