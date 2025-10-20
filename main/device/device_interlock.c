#include "device_interlock.h"
#include "wiegand.h"
#include "client.h"
#include "signal.h"
#include "bsp.h"
#include "log.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <string.h>

#define ILOCK_POWER_UPDATE_PERIOD 10U //s

typedef struct {
    char id[ILOCK_SESS_ID_BYTES_MAX];
    int kwh; //": 0,
    card_t card; // ID card that started the interlock session
} interlock_session_t;

typedef struct {
    const config_interlock_t *config;
    wieg_evt_handle_t evt_handle;
    interlock_session_t session;
    TimerHandle_t power_update;
} ilock_ctx_t;

static status_t interlock_init(const config_t *config);
static status_t interlock_deinit(void);
static void interlock_handle_swipe(wieg_evt_t event, card_t *card, void *ctx);
static void ilock_end_session(interlock_session_t *sess);
static bool ilock_has_session(const interlock_session_t *sess);
static bool ilock_session_match(const interlock_session_t *sess, char *id);
static void ilock_pwr_update_cb(TimerHandle_t xTimer);
void ilock_power_control(const config_interlock_t *config, bool on);
static status_t client_cmd_handler(msg_t *msg);

static ilock_ctx_t _ctx;

const device_t ilock = {
    .init = interlock_init,
    .deinit = interlock_deinit,
};

static status_t interlock_init(const config_t *config)
{
    memset(&_ctx.session, 0x00, sizeof(interlock_session_t));
    _ctx.config = &config->interlock;
    _ctx.evt_handle = wieg_evt_handler_reg(WIEG_EVT_NEWCARD, interlock_handle_swipe, (void *)&_ctx);

    // Register cb for server requests
    client_handler_register(client_cmd_handler);

    _ctx.power_update = xTimerCreate(
        "Power Update", 
        pdMS_TO_TICKS(1000 * ILOCK_POWER_UPDATE_PERIOD), 
        true, 
        NULL, 
        ilock_pwr_update_cb
    );
    if (_ctx.power_update == NULL) { return -STATUS_NOMEM; }

    return STATUS_OK;
}

static status_t interlock_deinit(void)
{
    // TODO: does this finish everything before function has exited?
    ilock_end_session(&_ctx.session); 
    return STATUS_OK;  
}

static void interlock_handle_swipe(wieg_evt_t event, card_t *card, void *ctx)
{
    ilock_ctx_t *ilock_ctx = (ilock_ctx_t *) ctx;

    if (!ilock_has_session(&ilock_ctx->session))
    {
        // request a new interlock session

        msg_t msg = {
            .type = MSG_ILOCK_SESS_START,
            .ilock_start_req.card_id = card->raw,
        };
        ilock_ctx->session.card.raw = card->raw;

        status_t status = client_send_msg(&msg);
        if (STATUS_OK != status)
        {
            ERROR("Couldn't start interlock session with card %u: %d", card->raw, status);
            signal_alert();

        }
    }
    else if (0 == strcmp("system", ilock_ctx->session.id))
    {
        // turn off the interlock if it was manually turned on by the system

        msg_t msg = {
            .type = MSG_ILOCK_OFF
        };

        status_t status = client_send_msg(&msg);
        if (STATUS_OK != status)
        {
            ERROR("Failed to turn off interlock (%u): %d", card->raw, status);
            signal_alert();
        }
        ilock_end_session(&ilock_ctx->session);

    }
    else
    {
        // end the current interlock session
        ilock_end_session(&ilock_ctx->session);
    }
}

static bool ilock_has_session(const interlock_session_t *sess)
{
    return 0 != strlen(sess->id);
}

static bool ilock_session_match(const interlock_session_t *sess, char *id)
{
    return 0 == strcmp(id, sess->id);
}

static void ilock_end_session(interlock_session_t *sess)
{
    // Do we have a session right now, and is it unique from "system?"
    if (ilock_has_session(sess) && !ilock_session_match(sess, "system"))
    { 
        INFO("Ending session ID %s", sess->id);
    
        msg_t msg;
        msg.type = MSG_ILOCK_SESS_END;
        msg.ilock_end.card_id = sess->card.raw;
        msg.ilock_end.session_kwh = sess->kwh;
        memcpy( msg.ilock_end.session_id, sess->id, ILOCK_SESS_ID_BYTES_MAX);
        
        status_t status = client_send_msg(&msg);
        if (STATUS_OK != status)
        {
            ERROR("Couldn't end interlock session %s: %d", sess->id, status);
        }

        sess->kwh = 0;
        sess->card.raw = 0;
        memset(sess->id, 0x00, ILOCK_SESS_ID_BYTES_MAX);

        ilock_power_control(_ctx.config, false);
        // TODO: RGB led state
    }
}

void ilock_power_control(const config_interlock_t *config, bool on)
{
    if (on)
    {
        // 
        if(0 < strlen(config->tasmota_host))
        {
            // TODO: Send request to tasmota: power on
        }
        else
        {
            gpio_out_set(OUTPUT_LOCK, true);
        }
    }
    else
    {
        // 
        if(0 < strlen(config->tasmota_host))
        {
            // TODO: Send request to tasmota: power off
        }
        else
        {
            gpio_out_set(OUTPUT_LOCK, false);
        }
    }
}

static void ilock_pwr_update_cb(TimerHandle_t xTimer)
{
    // TODO check if client is connected

    if (ilock_has_session(&_ctx.session) && !ilock_session_match(&_ctx.session, "system"))
    {
        // TODO: get interlock power usage
        msg_t msg;
        msg.type = MSG_ILOCK_SESS_UPDATE;
        msg.ilock_update.session_kwh = _ctx.session.kwh;
        memcpy(msg.ilock_update.session_id, _ctx.session.id, ILOCK_SESS_ID_BYTES_MAX);
        status_t status = client_send_msg(&msg);
        if (STATUS_OK != status)
        {
            ERROR("Failed to send interlock update %s: %d", _ctx.session.id, status);
        }
    }
}

static status_t client_cmd_handler(msg_t *msg)
{
    status_t status = -STATUS_UNAVAILABLE;

    if (msg->type == MSG_UNLOCK)
    {
        WARN("Turning on interlock from manual request!");
        ilock_power_control(_ctx.config, true);
        strcpy(_ctx.session.id, "system");
        signal_action(); 

        status = STATUS_OK;
    }
    if (msg->type == MSG_LOCK)
    {
        WARN("Turning off interlock from manual request!");
        ilock_end_session(&_ctx.session);
       
        status = STATUS_OK;
    }
    if (msg->type == MSG_ILOCK_SESS_START)
    {
        WARN("Turning on interlock from new session!");
        memcpy(_ctx.session.id, msg->ilock_start_rsp.session_id, ILOCK_SESS_ID_BYTES_MAX);
        _ctx.session.kwh = 0;

        ilock_power_control(_ctx.config, true);
        signal_action();

        status = STATUS_OK;
    }
    if (msg->type == MSG_ILOCK_SESS_REJECTED)
    {
        WARN("Interlock session request failed!");
        signal_alert();

        status = STATUS_OK;
    }
    if (msg->type == MSG_ILOCK_SESS_UPDATE)
    {
        WARN("Interlock session update requested - UNIMPLEMENTED");
        
        status = STATUS_OK;
    }

    return status;
}
