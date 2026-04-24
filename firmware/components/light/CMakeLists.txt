idf_component_register(SRCS 
                        "src/beacon.c"
                        "src/light.c"
                        "src/outdoor.c"
                    INCLUDE_DIRS "include"
                    PRIV_REQUIRES
                        esp_driver_gpio
                        esp_driver_gptimer
                        esp_driver_ledc
                        persistence
                    )
