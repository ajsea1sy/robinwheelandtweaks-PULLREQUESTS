# Secondary Inventory Wheel

A Dusklight code mod that adds a second item wheel with 24 mod-owned item slots and a separate Goron Tunic slot. It reuses the vanilla item-wheel presentation while handling slot identity, selection, movement, and rendering independently of vanilla inventory indexes.

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

The secondary wheel has 24 mod-owned item slots (`0x80` through `0x97`) plus a dedicated Goron Tunic identity (`0xFE`). These identities index the mod's private slot storage and are not vanilla save inventory indexes. The tunic is always displayed as its own wheel entry.

## License

Released under the MIT License. See [`LICENSE`](LICENSE).
