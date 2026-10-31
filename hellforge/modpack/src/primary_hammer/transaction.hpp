#pragma once
#include "catalog.hpp"
#include <functional>
namespace hammer {
struct Io {
 std::function<int(uintptr_t,void*,size_t)> read;
 std::function<int(uintptr_t,const void*,size_t)> write;
 std::function<bool(uintptr_t,size_t)> dedicated;
 Bytes bytes(uintptr_t at,size_t n)const{require(at>=65536 && n && n<=4194304,"read bounds");Bytes b(n);require(!read(at,b.data(),n),"read failed");return b;}
 template<class T>T value(uintptr_t at)const{return get<T>(bytes(at,sizeof(T)),0);}
};
struct Edit{uintptr_t at;Bytes before,after;};
struct Transaction {
 Io& io;std::vector<Edit> edits;std::vector<std::pair<uintptr_t,Bytes>> observed;
 void observe(uintptr_t at,size_t n){observed.push_back({at,io.bytes(at,n)});}
 void observations()const{for(auto& o:observed)require(io.bytes(o.first,o.second.size())==o.second,"observed ownership changed");}
 void add(uintptr_t at,Bytes after){for(auto& e:edits)require(at+after.size()<=e.at || e.at+e.after.size()<=at,"overlapping writes");edits.push_back({at,io.bytes(at,after.size()),std::move(after)});}
 void commit(){
  observations();
  for(auto& e:edits)require(io.bytes(e.at,e.before.size())==e.before,"external change before transaction");
  size_t attempted=0;
  try{
   for(auto& e:edits){observations();require(io.bytes(e.at,e.before.size())==e.before,"external change during transaction");++attempted;require(!io.write(e.at,e.after.data(),e.after.size()) && io.bytes(e.at,e.after.size())==e.after,"write/readback failed");}
  }catch(...){
   bool ok=true;
   while(attempted){auto& e=edits[--attempted];auto now=io.bytes(e.at,e.before.size());
    // A failed API write may leave either complete value. Preserve foreign bytes.
    if(now==e.before)continue;
    if(now!=e.after || io.write(e.at,e.before.data(),e.before.size()) || io.bytes(e.at,e.before.size())!=e.before)ok=false;
   }
   require(ok,"rollback incomplete; restart required");throw;
  }
 }
};
}
