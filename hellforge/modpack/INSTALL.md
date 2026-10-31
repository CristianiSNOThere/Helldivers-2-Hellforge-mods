# Super Earth Arsenal native pack

Super Earth Arsenal modpack for HellForge SDK 13 or newer and game build 25480438.
Choose from 26 independent options and 29 One True Flag damage presets.
Python 3.10 or later configures the selection. PowerShell 7 and HellForge's
pinned compiler prepare the local assets.

1. Close the game. Extract the source pack into a new folder.
2. Set each option to `true` or `false` in `pack-config.json`. The Flag's
   `normal` and `durable` values select its damage preset. All 26 options are
   listed; names and behavior are in [PLAYER-GUIDE.md](PLAYER-GUIDE.md).
3. Create a new configured source folder:

   ```powershell
   python tools/configure_pack.py --config pack-config.json --out "C:/Mods/Arsenal-selected"
   ```

4. Prepare that selection's assets from your own supported HellForge copy:

   ```powershell
   pwsh -File "C:/Mods/Arsenal-selected/tools/prepare_assets.ps1" -InstallRoot "C:/HellForge" -ModRoot "C:/Mods/Arsenal-selected" -Out "C:/Mods/Arsenal-local-assets"
   ```

   Both output folders must be new. Preparation checks the game inputs and
   generated resources. It does not install or enable the mod.

5. Build the configured selection with HellForge’s SDK and bundled compiler:

   ```powershell
   python "C:/Mods/Arsenal-selected/tools/build_native.py" --install "C:/HellForge"
   ```

   This builds the C and C++ sources and creates the DLL for game build 25480438.
6. Copy the compiled selection into HellForge's `mods` folder. Copy the
   contents of `Arsenal-local-assets/data` into that mod's `data` folder. Keep
   any previous installation as a backup outside `mods`.
7. Enable the native pack in HellForge and launch. HellForge loads the included
   game-specific DLL. Use this native pack in place of the Arsenal Lua pack
   for the same session.

To change a selection, close the game and repeat configuration and local asset
preparation into new folders. Replace the installed selection yourself so that
disabled options do not retain assets from an earlier selection.

To remove the pack, close the game and disable or move its folder out of
HellForge's `mods` directory. The pack does not change the Steam installation.

Keep your current Scope of Authority Backport installation and settings. The pack's options and both Sickle heatsinks remain available with the backport enabled.

Gas Overhaul can be used with the 150-health Health Rework. Keep your chosen health, regeneration and stim settings.

The CQC-20 option replaces the modpack’s support-weapon version with the primary hammer. Disable the separate Primary Breaching Hammer mod when using this version.
For a source rebuild after composing a selection, run `python tools/build_native.py --install PATH_TO_HELLFORGE`. The package includes its full source and a DLL pinned to game build 25480438.
