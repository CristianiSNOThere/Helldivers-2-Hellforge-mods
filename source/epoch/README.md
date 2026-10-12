# Epoch source

The release ZIP contains the complete Lua and animation resources. `epoch-entry.lua` is the exact released Lua payload; `epoch-core.lua` and `spec.json` contain its guarded transaction and data specification. The workspace builder is `work/epoch-20261012/build_candidate_v012.py`; production packaging is `work/release-arc-epoch-20261012/prepare_release.py`. These scripts require the workspace extraction and existing archive/runtime helpers. Preserve shared animation payload equality with AC-8 when maintaining either package.
