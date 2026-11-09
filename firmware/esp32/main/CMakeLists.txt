idf_component_register(
    SRCS
        "main.cpp"

        "app/app.cpp"

        "matter/matter_manager.cpp"

        "sensors/sensor_manager.cpp"

        "led/led_manager.cpp"

    INCLUDE_DIRS
        "."
        "app"
        "matter"
        "sensors"
        "led"

    REQUIRES
        esp_matter
        led_strip
        nvs_flash
)