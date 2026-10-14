# SAI Focus Precision v0.10 release source

`entry.lua` is the exact shipped Lua resource, also mirrored at `source/startup-initialization/8ba5fd7bc87570d8.lua`. Its module identity is `mods/codex/sai_focus_precision`, resource hash `8BA5FD7BC87570D8`, Lua type `A14E8DFA2CD117E2`; package GUID `3c8fb9f1-f1c2-4c81-bf48-a97722c1e902`.

Place the UTF-8 Lua payload in the Shared Loader version-2 envelope: little-endian u32 payload length, u32 version2, then payload bytes. Preserve the pure Lua archive and stable package/module identity. Check source row hashes, unique resource ownership, native code pins, source snapshots, bounded retries, transactions and reverse rollback. The attachment transaction performs two edits in one snapshot and records the expected preceding snapshot for each operation.

The final workspace builder is `work/sai-rebalance-20261014/prepare_final_release.py`, deriving only the requested50-percent spread adjustment from gameplay-accepted rev7. `verify_final_release.py` uses45 isolated LuaJIT fixtures and audits all20individual mods against the combined pack. It requires the bundled Python, the game's lua51.dll, retained extracted source data and workspace archive helpers. Use a fresh output folder for changed builds. Do not run older preparation builders embedded as historical source.

When SAI's reported settings change, extend Gas Overhaul's exact peer recognition and build the matching individual/pack together. Gas Overhaul's current exact shipped source is `source/gas-overhaul/sterilizer-entry.lua`, mirrored in `source/startup-initialization/e54e34210bac937e.lua`.
