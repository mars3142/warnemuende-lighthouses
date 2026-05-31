idf_component_register(SRCS
                        "src/thread_control.c"
                    INCLUDE_DIRS "include"
                    REQUIRES
                        openthread
                    PRIV_REQUIRES
                        nvs_flash
                        light
)
