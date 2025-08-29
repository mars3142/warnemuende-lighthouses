idf_component_register(SRCS 
                        "beacon.c"
                        "light.c"
                        "outdoor.c"
                    INCLUDE_DIRS "include"
                    PRIV_REQUIRES
                        esp_driver_gpio
                        esp_driver_gptimer
                        esp_driver_ledc
                        persistence
                    )
