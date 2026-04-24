idf_component_register(SRCS
                        "src/matter.cpp"
                    INCLUDE_DIRS "include"
                    PRIV_REQUIRES
                        espressif__esp_matter
                        light
)
