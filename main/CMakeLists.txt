file(GLOB_RECURSE IMAGE_SOURCES images/*.c)
set(COMPONENT_SRCS
    main.c
    httpService.c
    wifi_api.c
    openai.c
    EMBED_TXTFILES ocicert.pem azurecert.pem azuresttcert.pem openai.pem 
    )

set(COMPONENT_ADD_INCLUDEDIRS 
    include
    )
#set(EMBED_TXTFILES ocicert.pem )

set(COMPONENT_REQUIRES
    esp-sr
    led_strip
    hardware_driver
    nvs_flash
    esp_http_client
    esp-tls
    log
    player
    )

register_component()



