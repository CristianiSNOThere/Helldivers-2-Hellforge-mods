#include "hellforge_mod.h"
int enemy_charger_init(const HfApi *,HfMod *);
int enemy_health_init(const HfApi *,HfMod *);
int enemy_overseer_init(const HfApi *,HfMod *);
int enemy_movement_init(const HfApi *,HfMod *);
int enemy_commander_init(const HfApi *,HfMod *);
int enemy_accuracy_init(const HfApi *,HfMod *);
HF_MOD_EXPORT int hellforge_mod_init(const HfApi *api,HfMod *mod)
{
    if(enemy_health_init(api,mod)) return 1;
    if(enemy_movement_init(api,mod)) return 1;
    if(enemy_overseer_init(api,mod)) return 1;
    if(enemy_charger_init(api,mod)) return 1;
    if(enemy_commander_init(api,mod)) return 1;
    return enemy_accuracy_init(api,mod);
}
