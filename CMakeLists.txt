cmake_minimum_required(VERSION 3.25)
project(secondary_inventory_wheel CXX)

add_mod(secondary_inventory_wheel
    FEATURES game
    # Feature .cpp modules are included by mod.cpp to share its private runtime state.
    SOURCES  src/mod.cpp
    MOD_JSON mod.json
    RES_DIR  res
)
