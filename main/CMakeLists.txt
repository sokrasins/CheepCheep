idf_component_register(
SRCS 
    "main.c"
    "bsp/fs.c"
    "bsp/gpio.c"
    "bsp/uptime.c"
    "bsp/sys.c"
    "config/config.c"
    "config/config_defaults.c"
    "wiegand/wiegand.c"
    "wiegand/wiegand_fmt.c"
    "nvstate/nvstate.c"
    "device/device_door.c"
    "device/device_interlock.c"
    "device/device_vending.c"
    "tags/tags.c"
    "signal/signal.c"
    "client/net.c"
    "client/ws.c"
    "client/client.c"
    "client/msg.c"
    "client/ota_dfu.c"
    "console/console.c"
    "util/log.c"
REQUIRES 
    freertos 
    esp_wifi 
    esp_driver_gpio 
    json 
    console
    esp_http_client
    esp_https_ota
    app_update
    nvs_flash
INCLUDE_DIRS 
    "."
    "wiegand"
    "bsp"
    "util"
    "config"
    "nvstate"
    "device"
    "tags"
    "signal"
    "client"
    "console"
    )

target_compile_options(${COMPONENT_LIB} PRIVATE "-Wall" "-Wextra" "-Werror")

# This will include the contents of the folder into the file system. This only 
# applies to the binary for flashing, not the dfu-capable image.
#littlefs_create_partition_image(storage ../flash_data FLASH_IN_PROJECT)