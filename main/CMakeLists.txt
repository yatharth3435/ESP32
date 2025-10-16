idf_component_register(SRCS "main.c"
                    INCLUDE_DIRS "."
                    REQUIRES nvs_flash wifi_provisioning esp_http_client esp_timer lwip)