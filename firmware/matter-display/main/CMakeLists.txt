idf_component_register(
    SRCS "main.c" "ui.c" "kni_logo.c"
    INCLUDE_DIRS "."
    REQUIRES esp32s3-waveshare27690-bsp esp_timer mhz19
)
