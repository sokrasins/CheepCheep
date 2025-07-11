#ifndef CONFIG_TYPES_H_
#define CONFIG_TYPES_H_

#include "log.h"
#include <stdbool.h>

#define WIFI_POW_MAX 20 //dBm

// String byte lengths
#define CONFIG_PORTAL_WS_URL_BYTES      128
#define CONFIG_PORTAL_API_SECRET_BYTES  42
#define CONFIG_NET_SSID_BYTES           64
#define CONFIG_NET_PASS_BYTES           64
#define CONFIG_NET_COUNTRY_BYTES        3
#define CONFIG_DFU_URL_BYTES            128
#define CONFIG_ILOCK_HOST_BYTES         128
#define CONFIG_ILOCK_USER_BYTES         64
#define CONFIG_ILOCK_PASS_BYTES         64

// Device types
typedef enum {
    DEVICE_DOOR,
    DEVICE_INTERLOCK,
    DEVICE_VENDING,
    DEVICE_NONE,
} device_type_t;

// Vending relay behavior
typedef enum {
    VENDING_NONE,
    VENDING_HOLD,
    VENDING_TOGGLE,
} vending_mode_t;

// Portal configuration
typedef struct {
    char ws_url[CONFIG_PORTAL_WS_URL_BYTES];
    char api_secret[CONFIG_PORTAL_API_SECRET_BYTES];
} config_portal_t;

// Network configuration
typedef struct {
    char wifi_ssid[CONFIG_NET_SSID_BYTES];
    char wifi_pass[CONFIG_NET_PASS_BYTES];
    char wifi_country_code[CONFIG_NET_COUNTRY_BYTES];
    int wifi_power; // range: 2-20 dBm
} config_network_t;

// DFU configuration
typedef struct {
    bool enabled;
    char url[CONFIG_DFU_URL_BYTES];
    bool skip_cn_check;
    bool skip_version_check;
} config_dfu_t;

// General configuration
typedef struct {
    bool lock_reversed;
    bool reader_led_reversed;
    bool relay_reversed;
    bool door_sensor_reversed;
    bool door_sensor_enabled;
    int door_sensor_timeout;
    int door_open_alarm_timeout;
    bool out_1_reversed;
    bool in_1_reversed;
    bool aux_1_reversed;
    bool aux_2_reversed;
    int fixed_unlock_delay;
    int rgb_led_count;
    bool wiegand_enabled;
    bool uid_32bit_mode;
} config_general_t;

// Buzzer config
typedef struct {
    bool enabled;
    bool reversed;
    bool buzz_on_swipe;
    int action_delay;
} config_buzzer_t;

// Interlock config
typedef struct {
    char tasmota_host[CONFIG_ILOCK_HOST_BYTES];
    char tasmota_user[CONFIG_ILOCK_USER_BYTES];
    char tasmota_pass[CONFIG_ILOCK_PASS_BYTES];
} config_interlock_t;

// Vending config
typedef struct {
    int price;
    vending_mode_t mode;
    int toggle_time;
} config_vending_t;

// LCD config
typedef struct {
    bool enable;
    int address;
    int rows;
    int cols;
} config_lcd_t;

// Pin config
typedef struct {
    int aux_1;
    int aux_2;
    int rgb_led;
    int status_led;
    int reader_led;
    int reader_buzzer;
    int relay;
    int lock;
    int door_sensor;
    int out_1;
    int in_1;
    int sda;
    int scl;
    int uart_rx;
    int uart_tx;
    int wiegand_zero;
    int wiegand_one;
} config_pins_t;

// Development config
typedef struct {
    log_level_t log_level;
} config_dev_t;

// Client configs
typedef struct {
    config_portal_t portal;
    config_network_t net;
    config_dfu_t dfu;
} config_client_t;

// Total device config
typedef struct {
    device_type_t device_type;
    config_client_t client;
    config_general_t general;
    config_buzzer_t buzzer;
    config_interlock_t interlock;
    config_vending_t vending;
    config_lcd_t lcd;
    config_pins_t pins;
    config_dev_t dev;
} config_t;

#endif /*CONFIG_TYPES_H_*/