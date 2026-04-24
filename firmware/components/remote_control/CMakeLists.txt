idf_component_register(SRCS 
                        "src/char_desc.c"
                        "src/device_service.c"
                        "src/light_service.c"
                        "src/remote_control.c"
                        "src/uart_service.c"
                    INCLUDE_DIRS "include"
                    PRIV_REQUIRES
                        bt
                        esp_app_format
                        light
                        persistence
)
