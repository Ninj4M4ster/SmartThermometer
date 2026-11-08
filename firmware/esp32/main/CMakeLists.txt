idf_component_register(SRCS "main.cpp" "matter_init.cpp"
                    INCLUDE_DIRS "."
                    PRIV_REQUIRES esp_matter led_strip nvs_flash)
