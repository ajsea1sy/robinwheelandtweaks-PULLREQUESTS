# RobinWheelAndTweaks [By RobinTheSoundsmith]
Quick Link: `urlvanish.com/robinwheel`

This Dusklight code mod adds a second item wheel with 24 mod-owned item slots and a separate Goron Tunic slot. It reuses the vanilla item-wheel presentation while handling slot identity, selection, movement, and rendering independently of vanilla inventory indexes. Hawk Grass is provided in the first secondary slot and Hover Boots in the second; both can be assigned to X or Y.

## Repository contents

- `src/mod.cpp` contains the mod implementation.
- `mod.json` contains the Dusklight mod manifest.
- `CMakeLists.txt` defines the mod target for Dusklight's CMake build.
- `res/` is the mod's resource directory. The wheel loads its presentation from Dusklight at runtime and includes a custom winged Hover Boots icon.

## Building

This mod is built from a Dusklight source checkout, which provides the SDK, headers, libraries, and `add_mod` CMake function. Place this directory at `mods/secondary_inventory_wheel` and ensure the checkout's top-level `CMakeLists.txt` includes:

```cmake
add_subdirectory(mods/secondary_inventory_wheel)
```

Configure Dusklight as usual, then build the `secondary_inventory_wheel_package` target. The build produces `secondary_inventory_wheel.dusk`.

## Slot model

The secondary wheel has 24 mod-owned item slots (`0x80` through `0x97`) plus a dedicated Goron Tunic slot (`0xFE`). Slot IDs and custom item IDs (`0xFB` through `0xFD`) are private to the mod and are never written into the game's vanilla X/Y item assignments. The two secondary assignments are saved independently in the mod configuration. Assigning a secondary item to X or Y leaves that button's vanilla assignment intact; while a secondary item is assigned, it takes precedence for that button's HUD icon and activation. Assigning a vanilla item to that same button from the vanilla wheel clears its private secondary assignment so the vanilla item immediately takes over. Hawk Grass uses its private item ID in the first slot and displays a custom Hawk Grass icon. Its assigned X/Y action enters the game's grass-whistle flow, retaining the native reed melody and hawk sound effects. After its attack and return flight finish, the Hawk called by modded Hawk Grass is removed after five seconds in idle with Link outside the Hawk interaction. Hover Boots use their private item ID in the second slot and a custom boots-with-wings icon in the wheel, X/Y preview, and HUD. The X/Y HUD icon is rendered directly from the mod resource, without using a vanilla item ID to load or scale it. Assign them to X or Y and press that button to toggle airwalking immediately; the Iron Boots equip animation plays on ground and in the air but does not gate the Hover Boots state change or modify vanilla Iron Boots. While active, the magnetic-boots particles follow Link’s foot joints immediately, including while he is still standing on solid ground. When Link walks or jumps off solid ground, Hover Boots keep him at the height where he left it and show a ripple beneath him. While airborne, the mod prevents vanilla auto-fall/ledge actions from replacing the current player action, including idle movement states, while allowing other mods' jump actions to run. At the hover plane it uses a one-shot landing animation and returns Link to movement without requiring a background polygon. The mod does not add an A-button jump. The normal A-button put-away behavior for vanilla held items is unchanged. After updating from older versions, assign secondary items again in the secondary wheel; the old implementation mirrored those assignments into vanilla item state.

The mod settings remain available in the Mods panel and can also be opened directly from the menu bar's **Robin's W&T** entry. The menu lets players choose which Left/Down D-pad direction opens the secondary wheel, walk on lava indefinitely with the Goron Tunic (without sinking or triggering the five-second lava return), toggle Twilight Ward, enable one-hit damage for attacks from the Hawk called by the secondary Hawk Grass, and toggle Hover Boots water walking. Twilight Ward is off by default and, while Link wears Magic Armor, normal and heavy hits still deal their usual damage but do not interrupt Link, stun him, or knock him back. Water walking is off by default and only applies while the secondary-wheel Hover Boots are equipped; it keeps movement on a flat water-level plane instead of using submerged floor slopes, drops, or ice-slip movement. It also suppresses submerged slide-polygon and slope checks and clears the vanilla ground-step offsets while the water plane is active. Enabling water walking replaces airwalking, so players can choose which Hover Boots behavior they prefer. With the one-hit option enabled, player targets take 20 hearts of damage. The modded Hawk's attack is classified as a thrown object, normal sword hit, and bomb for compatibility with enemy-specific damage handlers, including mounted Bulblins.

The menu also provides Rupee Magnet as an optional feature rather than a wheel item. Its radius defaults to 100% (300 game units, matching the game's existing rupee-return distance) and can be set from 50% (150 units) to 300% (900 units) in 25% steps. Lava walking, Twilight Ward, Hawk one-hit damage, Hover Boots water walking, and Rupee Magnet are off by default.

The secondary wheel's close behavior matches the vanilla wheel, including restoring background music and sound-effect levels when the wheel closes.

## License

Released under the MIT License. See [`LICENSE`](LICENSE).
