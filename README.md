# Ores Be Explodin'

A Satisfactory mod (mod ID `OresBeExplodin`) that automatically mines the loose ore deposits around you: the small boulders you normally mine by hand. Resource nodes for miners are not touched.

Walk past a deposit and it pops. Loot it, destroy it, or leave the Uranium alone: your call.

## Settings
Open Mods -> Mod Configuration -> Ores Be Explodin', from the main menu or the pause menu. Changes apply right away.

| Setting | What it does |
|---|---|
| Enabled | Turns auto-looting on or off without removing the mod. |
| Mode | See below. Default: Auto Loot All - Except Uranium. |
| Range (meters) | Deposits within this distance of a player are mined. 1 - 100, default 10. A foundation is 8 m wide. |
| Deposits to mine by hand first | Auto-looting starts once this many deposits have been mined out by hand in the save. 0 = right away. The count is kept per save. |

The line at the top of the settings shows whether the mod is active, off, or still waiting for you to mine a few deposits by hand.

### Modes
- **Auto Loot All - Warning: Uranium!** Mines every deposit in range into your inventory, Uranium included. Pack enough Iodine-Infused Filters.
- **Auto Destroy All.** Removes every deposit in range. You get nothing.
- **Auto Loot All - Except Uranium.** Mines everything and leaves Uranium deposits alone.
- **Auto Loot All - Destroy Uranium.** Mines everything, and removes Uranium deposits without giving you the Uranium.

"Uranium" means any radioactive resource, so radioactive ores added by other mods count too.

## Good to know
- When your inventory is full, looting stops and the rest of the deposit stays where it is.
- Nothing is mined while the game is paused.
- Multiplayer: the mod runs on the host and uses the host's settings. Not tested in multiplayer yet.
- Windows client only for now. No dedicated server builds.

## Requirements
- Satisfactory 1.2.6 (build 520140) or newer
- SML 3.12.1 or newer

## Building
Put this repo at `<project>\Mods\OresBeExplodin` in the SatisfactoryModLoader starter project (SML 3.12.1) and package it with Alpakit, or with `RunUAT.bat ... PackagePlugin -DLCName=OresBeExplodin`.
