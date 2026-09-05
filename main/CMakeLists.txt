idf_component_register(
    SRCS "main.c" "usage_model.c" "usage_font.c" "usage_capture.c" "usage_ui.c" "usage_font_small.c" "usage_font_medium.c" "usage_font_title.c" "usage_font_number.c" "usage_font_clock.c" "usage_font_token.c"
    INCLUDE_DIRS "."
    REQUIRES bsp esp_event esp_http_client esp_netif esp_wifi json mbedtls
             nvs_flash esp_driver_usb_serial_jtag esp_timer lwip
)
