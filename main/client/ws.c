#include "ws.h"
#include "net.h"
#include "log.h"

#include "esp_websocket_client.h"
#include "esp_crt_bundle.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

#include <stddef.h>
#include <string.h>

#define WS_MAX_LEN 1024U // bytes

typedef struct {
    ws_evt_cb_t cb;
    void *ctx;
} ws_evt_handler_t;

typedef struct {
    esp_websocket_client_handle_t client;
    char uri[64];
    ws_evt_handler_t handler;
    bool connected;
} ws_ctx_t;

// Connection state management
static void ws_evt_cb(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data);

static ws_ctx_t _ctx;
//static cJSON *msg = NULL;

static char *msg_bytes = NULL;
static size_t msg_len = 0;

status_t ws_init(char *url)
{
    //status_t status;

    _ctx.handler.cb = NULL;
    _ctx.connected = false;

    esp_log_level_set("websocket_client", ESP_LOG_DEBUG);

    strcpy(_ctx.uri, url);

    esp_websocket_client_config_t ws_cfg = {
        .uri = _ctx.uri,                    // Server uri
        .skip_cert_common_name_check = true,// For now skip verification, but this should change in production
        .crt_bundle_attach = esp_crt_bundle_attach, // Basic cert bundle for most common 
        .reconnect_timeout_ms = 10000,      // Default
        .network_timeout_ms = 10000,        // Default
        .ping_interval_sec = 0xFFFFFFFF,    // Disable the automated ping, the server expects a client-level ping-pong
        .task_stack = (8*1024),             // Bigger stack, default is (4*1024)
        //.buffer_size = 2048,                // Bigger message buffer
    };

    _ctx.client = esp_websocket_client_init(&ws_cfg);
    if (_ctx.client == NULL)
    {
        ERROR("Couldn't initialize the websocket client");
        return -STATUS_IO;
    }

    esp_err_t err = esp_websocket_register_events(_ctx.client, WEBSOCKET_EVENT_ANY, ws_evt_cb, (void *)&_ctx.client);
    if (err != ESP_OK)
    {
        ERROR("Unable to register websocket event handler: %s", esp_err_to_name(err));
        return -STATUS_IO;
    }

    return STATUS_OK;
}

status_t ws_deinit(void)
{
    esp_websocket_client_destroy(_ctx.client);
    return STATUS_OK;  
}

status_t ws_open(void)
{
    esp_err_t err = esp_websocket_client_start(_ctx.client);
    if (err != ESP_OK)
    {
        ERROR("Couldn't open websocket: %s", esp_err_to_name(err));
    }
    return STATUS_OK;
}

status_t ws_close(void)
{
    esp_websocket_client_close(_ctx.client, pdMS_TO_TICKS(3000));
    return STATUS_OK;
}

status_t ws_evt_cb_register(ws_evt_cb_t cb, void *ctx)
{
    if (_ctx.handler.cb == NULL)
    {
        _ctx.handler.cb = cb;
        _ctx.handler.ctx = ctx;
        return STATUS_OK;
    }

    return -STATUS_NOMEM;
}

status_t ws_send(cJSON *msg)
{
    assert(msg);

    if (_ctx.connected)
    {
        if (_ctx.client == NULL)
        {
            ERROR("The client is NULL, but status indicates it's connected. Setting disconnected state");
            _ctx.connected = false;
            return -STATUS_NO_RESOURCE;
        }

        char *pkt = cJSON_PrintUnformatted(msg);
        if (pkt == NULL)
        {
            ERROR("Couldn't parse the json to be sent from websocket");
        }
        else
        {
            INFO("--> %.*s", strlen(pkt), pkt);
            int rc = esp_websocket_client_send_text(_ctx.client, pkt, strlen(pkt), portMAX_DELAY);
            free(pkt);
            if (rc < 0) // TODO: Check for rc < strlen(pkt)
            {
                ERROR("Couldn't send message");
                return -STATUS_IO;
            }
            return STATUS_OK;
        }
    }
    else
    {
        INFO("Websocket not connected, skipping send");
    }
    return -STATUS_NO_RESOURCE;
}

static void ws_evt_cb(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    esp_websocket_event_data_t *data = (esp_websocket_event_data_t *)event_data;
    switch (event_id) 
    {
    case WEBSOCKET_EVENT_BEGIN:
        break;

    case WEBSOCKET_EVENT_CONNECTED:
        INFO("Websocket Connected");
        _ctx.connected = true;
        if (_ctx.handler.cb != NULL)
        {
            _ctx.handler.cb(WS_OPEN, NULL, _ctx.handler.ctx);
        }
        break;

    case WEBSOCKET_EVENT_DISCONNECTED:
        INFO("Websocket Disconnected");
        _ctx.connected = false;
        ERROR("HTTP status code: %d",  data->error_handle.esp_ws_handshake_status_code);
        if (data->error_handle.error_type == WEBSOCKET_ERROR_TYPE_TCP_TRANSPORT) 
        {
            ERROR("reported from esp-tls: %d", data->error_handle.esp_tls_last_esp_err);
            ERROR("reported from tls stack: %d", data->error_handle.esp_tls_stack_err);
            ERROR("captured as transport's socket errno: %d",  data->error_handle.esp_transport_sock_errno);
        }
        if (_ctx.handler.cb != NULL)
        {
            _ctx.handler.cb(WS_CLOSE, NULL, _ctx.handler.ctx);
        }
        break;

    case WEBSOCKET_EVENT_DATA:
        if (data->op_code == 0x2) 
        {
            ERROR("Unexpected binary data");
        } 
        else if (data->op_code == 0x08 && data->data_len == 2) 
        {
            WARN("Received closed message with code=%d", 256 * data->data_ptr[0] + data->data_ptr[1]);
        } 
        else 
        {
            INFO("<-- %.*s", data->data_len, (char *)data->data_ptr);
        }

        // This message is either:
        // 1. The start of a new message, or
        // 2. the continuation of a fragmented message
        if (msg_bytes == NULL)
        {
            // Handle case 1. If we don't have a message buffer, make one.
            msg_bytes = calloc(WS_MAX_LEN, sizeof(uint8_t));
            msg_len = 0;
        }
        else
        {
            // Handle case 2. We need buffer space for more message, so 
            // embiggen our existing buffer
            // TODO: What's a realistic limit here? There should be a 
            // deterministic upper-bound
            msg_bytes = realloc(msg_bytes, msg_len + WS_MAX_LEN);
            if (msg_bytes == NULL)
            {
                ERROR("Unable to allocate larger buffer for JSON fragment");
            }
        }

        // Copy message into the working buffer
        memcpy(&msg_bytes[msg_len], data->data_ptr, data->data_len);

        // Try to parse a json payload. 
        // If we succeed, then send it to be parsed further. 
        // If we fail, triage.
        cJSON *msg = cJSON_Parse(msg_bytes);
        if (msg) 
        {
            // Send message to the client
            INFO("Websocket message parsed");
            if (_ctx.handler.cb != NULL)
            {
                _ctx.handler.cb(WS_MSG, msg, _ctx.handler.ctx);
            }

            // Throw away json memory
            cJSON_Delete(msg);
            msg = NULL;

            // Throw away raw binary memory
            free(msg_bytes);
            msg_bytes = NULL;
            msg_len = 0;
        }
        else
        {
            // Check where the parsing error occurred
            uint32_t err_idx = (uint32_t)cJSON_GetErrorPtr() - (uint32_t)msg_bytes;

            if (data->data_len == WS_MAX_LEN && err_idx == data->data_len)
            {
                // If the message length is the max size from the websocket, 
                // AND the parsing error is at the end of the message, we think 
                // we're looking at a fragmented message. In this case, we'll 
                // retain the data we collected and wait for a new message.
                WARN("Fragmented message detected, waiting for next packet");
                msg_len += WS_MAX_LEN;
            }
            else
            {
                // Otherwise, the data is just corrupt. 
                // TODO: We could additionally try to parse JUST the new message.
                ERROR("Fail to parse JSON");
                ERROR("    json len: %d", data->data_len);
                ERROR("    error offset: %ld", err_idx);

                // Throw away raw binary memory
                msg_len = 0;
                free(msg_bytes);
                msg_bytes = NULL;
            }

        }
        break;

    case WEBSOCKET_EVENT_ERROR:
        ERROR("Websocket Error");
        ERROR("HTTP status code: %d",  data->error_handle.esp_ws_handshake_status_code);
        if (data->error_handle.error_type == WEBSOCKET_ERROR_TYPE_TCP_TRANSPORT) 
        {
            ERROR("reported from esp-tls %d", data->error_handle.esp_tls_last_esp_err);
            ERROR("reported from tls stack %d", data->error_handle.esp_tls_stack_err);
            ERROR("captured as transport's socket errno %d",  data->error_handle.esp_transport_sock_errno);
        }
        break;

    case WEBSOCKET_EVENT_FINISH:
        WARN("WEBSOCKET_EVENT_FINISH");
        // TODO: event here
        // this is sent from the server when the device successfully 
        // connects, but isn't authorized.
        break;
    }
}
