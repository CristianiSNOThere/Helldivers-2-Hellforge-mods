#pragma once
#include <cstdint>
namespace hammer {
// One repair opportunity per fresh avatar/unit; never repeat a draw every tick.
struct WieldPolicy {
 uint32_t avatar=0,unit=0;float age=0,stable=0;bool done=false;
 bool observe(uint32_t a,uint32_t u,float dt,bool foreign_held,bool eligible,bool already_wielded){
  if(a!=avatar || u!=unit){avatar=a;unit=u;age=stable=0;done=false;}
  if(done)return false;
  if(!(dt>0 && dt<=1)){stable=0;return false;}
  age+=dt;
  if(foreign_held || already_wielded || age>30){done=true;return false;}
  stable=eligible?stable+dt:0;
  if(age>=3 && stable>=.75f){done=true;return true;}
  return false;
 }
};
}
