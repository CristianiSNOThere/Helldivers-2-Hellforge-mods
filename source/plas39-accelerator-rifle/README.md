# PLAS-39 source v1.1.10

`implementation.lua` is the fully resolved source of the published individual and pack implementation, compiled with the game LuaJIT and chunk name `@plas39_accelerator_firemode.lua`. `resolved-mode.lua` and `resolved-native.lua` are its exact component sources. The internal logging label remains v1.1.8 to preserve the accepted compiled payload.

Production packaging: `work/plas39-regression-20261012/prepare_release.py` in the HD2 Mods workspace, using the accepted individual and pack ZIPs. Candidate build/verification: `work/plas39-regression-20261012/build_candidate.py`. Existing older builders and templates are historical and must not regenerate the current release. Scripts need workspace extraction, current base ZIPs, archive helpers and the game LuaJIT. Keep unknown-interval refusal, native multiplier recognition, owned-instance isolation and rollback.
