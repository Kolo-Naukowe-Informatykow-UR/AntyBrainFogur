cmake_minimum_required(VERSION 3.16)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(matter-display)

# Force AntyBrainFogUR palette — overrides any managed component defaults
idf_build_set_property(COMPILE_DEFINITIONS
    "KNI_UI_C_BG=0x0A0E12"
    "KNI_UI_C_SURFACE=0x13181F"
    "KNI_UI_C_CARD=0x13181F"
    "KNI_UI_C_TEXT=0xE8EEF2"
    "KNI_UI_C_SUBTEXT=0x6B7E8F"
    "KNI_UI_C_TEAL=0x5EE6C1"
    "KNI_UI_C_TEAL_DIM=0x3BA88A"
    "KNI_UI_C_BLUE=0x7AA8FF"
    "KNI_UI_C_ALERT=0xFF8A6B"
    "KNI_UI_C_BORDER=0x1C2530"
    "KNI_UI_C_BAR=0x0D1318"
    "KNI_UI_C_INACTIVE=0x3D5068"
    APPEND
)
