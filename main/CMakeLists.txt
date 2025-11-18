idf_component_register(SRCS "main.c" "storage.c" 
                            "wifi_ap.c" "web_server.c" "error.c" "ble.c"
                    INCLUDE_DIRS "."
                    EMBED_TXTFILES "certs/cacert.pem" "certs/prvtkey.pem"
                    REQUIRES nvs_flash spiffs esp_http_server esp_https_server esp_wifi json esp_driver_gpio bt)