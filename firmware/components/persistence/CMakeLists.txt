idf_component_register(SRCS 
                        "src/persistence.c"
                    INCLUDE_DIRS "include"
                    PRIV_REQUIRES
                        nvs_flash
)
