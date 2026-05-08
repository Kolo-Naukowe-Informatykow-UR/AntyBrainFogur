cmake_minimum_required(VERSION 3.16)

# ── esp-matter component path ────────────────────────────────────────────
# Must be appended BEFORE IDF project.cmake is included so that the build
# system can discover the esp_matter and connectedhomeip components.
if(DEFINED ENV{ESP_MATTER_PATH})
    set(_CHIP_ROOT "$ENV{ESP_MATTER_PATH}/connectedhomeip/connectedhomeip")

    # esp_matter/CMakeLists.txt reads ${MATTER_SDK_PATH} (a plain CMake variable).
    # export.sh may leave it unset when install.sh did not complete cleanly,
    # so we force it here as a cache variable visible to all component CMakeLists.
    set(MATTER_SDK_PATH "${_CHIP_ROOT}" CACHE PATH "connectedhomeip root" FORCE)

    list(APPEND EXTRA_COMPONENT_DIRS "$ENV{ESP_MATTER_PATH}/components")
    list(APPEND EXTRA_COMPONENT_DIRS "${_CHIP_ROOT}/config/esp32/components")

    message(STATUS "esp-matter path  : $ENV{ESP_MATTER_PATH}")
    message(STATUS "MATTER_SDK_PATH  : ${MATTER_SDK_PATH}")
else()
    message(WARNING "ESP_MATTER_PATH is not set — Matter support will be unavailable")
endif()

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(matter-display)

# ── Matter C++ build flags (same pattern as every esp-matter example) ────────
# CHIP_HAVE_CONFIG_H tells SystemConfig.h to include the GN-generated
# SystemBuildConfig.h (which defines CHIP_SYSTEM_CONFIG_USE_LWIP = 1).
# Without this flag the #error "REQUIRED: CHIP_SYSTEM_CONFIG_USE_LWIP" fires.
if(DEFINED ENV{ESP_MATTER_PATH})
    idf_build_set_property(CXX_COMPILE_OPTIONS
        "-std=gnu++17;-DCHIP_HAVE_CONFIG_H;-Wno-overloaded-virtual"
        APPEND)

    # ── KNI / AntyBrainFogUR branding shown in Matter Basic Information
    # cluster (controllers like Apple Home, Google Home, Home Assistant
    # display these as Vendor / Model / Firmware version).  Override the
    # CHIP_DEVICE_CONFIG_DEVICE_* defaults from CHIPDeviceConfig.h.
    #
    # NOTE: no spaces inside the values — these defines also flow into the
    # GN build (chip static lib) via build_args.gn, and GN's argv parser
    # splits unquoted whitespace into separate tokens.  Stick to ASCII
    # without spaces (use - or _ if you need a separator).
    #
    # NOTE: VendorID stays 0xFFF1 — changing it would require a custom
    # signed DAC, which the example provider doesn't support.            ─
    idf_build_set_property(COMPILE_DEFINITIONS
        "CHIP_DEVICE_CONFIG_DEVICE_VENDOR_NAME=\"KNI_UR\""
        APPEND)
    idf_build_set_property(COMPILE_DEFINITIONS
        "CHIP_DEVICE_CONFIG_DEVICE_PRODUCT_NAME=\"AntyBrainFogUR_CO2\""
        APPEND)
    idf_build_set_property(COMPILE_DEFINITIONS
        "CHIP_DEVICE_CONFIG_DEFAULT_DEVICE_HARDWARE_VERSION_STRING=\"S3-LCD2.8_rev1\""
        APPEND)
    idf_build_set_property(COMPILE_DEFINITIONS
        "CHIP_DEVICE_CONFIG_DEVICE_SOFTWARE_VERSION_STRING=\"0.2.0-matter\""
        APPEND)
endif()

# ── AntyBrainFogUR palette — overrides any managed-component defaults ────
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
