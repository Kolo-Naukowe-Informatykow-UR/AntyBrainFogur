idf_component_register(
    SRCS
        "main.c"
        "ui.c"
        "kni_logo.c"
        "matter_app.cpp"
    INCLUDE_DIRS "."
    REQUIRES
        esp32s3-waveshare27690-bsp
        esp_timer
        mhz19
        esp_matter
        nvs_flash
)
