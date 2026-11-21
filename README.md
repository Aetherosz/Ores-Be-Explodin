# Auto Loot Ores

A Satisfactory mod (mod ID `AutoLootOres`) that automatically mines the loose ore deposits around you: the small boulders you normally mine by hand. Resource nodes for miners are not touched.

## Settings
Open Mods -> Mod Configuration -> Auto Loot Ores, from the main menu or the pause menu. Changes apply right away.

| Setting | What it does |
|---|---|
| Enabled | Turns auto-looting on or off. |
| Mode | See below. Default: Loot All Except Uranium. |
| Range (meters) | Deposits within this distance of a player are mined. 1 - 100, default 10. |
| Deposits to mine by hand first | Auto-looting starts once this many deposits have been mined out by hand in the save. 0 = right away. The count is kept per save. |

### Modes
- **Loot All**: mines every deposit in range into your inventory. Warning: that includes Uranium.
- **Destroy All**: removes every deposit in range. You get nothing.
- **Loot All Except Uranium**: mines everything and leaves Uranium deposits alone.
- **Loot All Destroy Uranium**: mines everything, and removes Uranium deposits without giving you the Uranium.

"Uranium" means any radioactive resource, so radioactive ores added by other mods count too.

## Good to know
- When your inventory is full, looting stops and the rest of the deposit stays where it is.
- Nothing is mined while the game is paused.
- Multiplayer: the mod runs on the host and works for every player in range; the host's settings count.
- Windows client only for now. No dedicated server builds.

## Building
Put this repo at `<project>\Mods\AutoLootOres` in the SatisfactoryModLoader starter project (SML 3.12) and package it with Alpakit, or with `RunUAT.bat ... PackagePlugin -DLCName=AutoLootOres`.
