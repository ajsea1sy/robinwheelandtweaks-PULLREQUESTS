# RobinWheelAndTweaks

Made by RobinTheSoundsmith. This Dusklight code mod adds a second item wheel with 24 mod-owned item slots and a separate Goron Tunic slot. It reuses the vanilla item-wheel presentation while handling slot identity, selection, movement, and rendering independently of vanilla inventory indexes. Hawk Grass is provided in the first secondary slot and can be assigned to X or Y to call a hawk.

## Repository contents

- `src/mod.cpp` contains the mod implementation.
- `mod.json` contains the Dusklight mod manifest.
- `CMakeLists.txt` defines the mod target for Dusklight's CMake build.
- `res/` is the mod's resource directory. No game assets are redistributed here; the wheel loads its presentation and item textures from Dusklight at runtime.

## Building

This mod is built from a Dusklight source checkout, which provides the SDK, headers, libraries, and `add_mod` CMake function. Place this directory at `mods/secondary_inventory_wheel` and ensure the checkout's top-level `CMakeLists.txt` includes:

```cmake
add_subdirectory(mods/secondary_inventory_wheel)
```

Configure Dusklight as usual, then build the `secondary_inventory_wheel_package` target. The build produces `secondary_inventory_wheel.dusk`.

## Slot model

The secondary wheel has 24 mod-owned item slots (`0x80` through `0x97`) plus a dedicated Goron Tunic identity (`0xFE`). These identities index the mod's private slot storage and are not vanilla save inventory indexes. The tunic is always displayed as its own wheel entry. Hawk Grass has a private item identity in the first slot and uses the vanilla Hawk Grass icon and held model. Its assigned X/Y item-use action enters the vanilla hawk-grass whistle flow rather than the Horse Flute flow, retaining the game's native reed melody and hawk sound effects. After its attack and return flight finish, the Hawk selected by modded Hawk Grass is removed after five seconds in idle with Link outside the Hawk interaction. Equipping a vanilla inventory item to X or Y clears any secondary-wheel assignment on that same button.

The mod menu lets players choose which Left/Down D-pad direction opens the secondary wheel, walk on lava indefinitely with the Goron Tunic (without sinking or triggering the five-second lava return), and enable one-hit damage for attacks from the Hawk called by the secondary Hawk Grass. With the one-hit option enabled, player targets take 20 hearts of damage. The modded Hawk's attack is classified as a thrown object, normal sword hit, and bomb for compatibility with enemy-specific damage handlers, including mounted Bulblins. The lava-walk and Hawk-damage options are off by default.

## License

Released under the MIT License. See [`LICENSE`](LICENSE).
