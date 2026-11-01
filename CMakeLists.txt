cmake_minimum_required(VERSION 3.25)
project(secondary_inventory_wheel CXX)

add_mod(secondary_inventory_wheel
    FEATURES game
    SOURCES  src/mod.cpp
    MOD_JSON mod.json
    RES_DIR  res
)
