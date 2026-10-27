# Install Enemy Rebalance

Requires HellForge SDK 13 or newer, HellForge game build 25480438 and Python 3.10 or later.

1. Extract the source ZIP into a new folder.
2. Prepare the animation files from your own updated Helldivers 2 game data. In a terminal, run:

   ```powershell
   python tools/prepare_assets.py --install-root "PATH_TO_HELLFORGE" --game-data "PATH_TO_UPDATED_HELLDIVERS_2/data" --out "NEW_OUTPUT_FOLDER"
   ```

3. Copy the generated `data` folder into the extracted Enemy Rebalance source folder.
4. Close the game. Place that complete folder in HellForge's `mods` folder, replacing the previous Enemy Rebalance folder or ZIP if installed. Keep one Enemy Rebalance copy enabled.
5. Launch through HellForge. It builds the C source with its bundled SDK and compiler.

The local asset tool reads your game files and writes only to a new output folder. The updated installation supplies the Charger's rear-lift braking animation. If that animation differs from the supported version, the tool stops before producing a usable data folder. Keep the two game installations separate: the updated copy supplies this animation; play through the supported HellForge copy.
