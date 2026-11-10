idf_component_register(
    SRCS
        "main.cpp"

        "app/app.cpp"
        "htu31/htu31.cpp"
        "sh1106/sh1106.cpp"

        "matter/matter_manager.cpp"

        "led/led_manager.cpp"

    INCLUDE_DIRS
        "."
        "app"
        "matter"
        "htu31"
        "sh1106"
        "led"

    REQUIRES
        esp_matter
        esp_driver_i2c
        led_strip
        nvs_flash
)