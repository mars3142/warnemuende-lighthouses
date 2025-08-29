idf_component_register(SRCS 
                        "char_desc.c"
                        "device_service.c"
                        "light_service.c"
                        "remote_control.c"
                        "uart_service.c"
                    INCLUDE_DIRS "include"
                    PRIV_REQUIRES
                        bt
                        esp_app_format
                        light
                        persistence
)
