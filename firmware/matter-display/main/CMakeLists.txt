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
        esp_netif        # IP info on Device Info screen + SNTP
        esp_app_format   # esp_app_get_description() for app/idf version
        esp_hw_support   # esp_read_mac()
        lwip             # esp_netif_sntp_init / esp_sntp.h
)
