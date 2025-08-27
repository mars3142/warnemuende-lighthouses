idf_component_register(SRCS 
                        "device_service.c"
                        "light_service.c"
                        "remote_control.c"
                    INCLUDE_DIRS "include"
                    PRIV_REQUIRES
                        bt
                        esp_app_format
                        light
)
