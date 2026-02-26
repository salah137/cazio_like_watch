idf_component_register(
                    SRCS "main.c"
                        "drawing.c"
                        "i2c_connection.c"
                        "utilitys.c"
                        
                    INCLUDE_DIRS "."
                    
                    REQUIRES  
                            esp_driver_i2c
                            esp_driver_gpio
                            esp_driver_gptimer
                    )
