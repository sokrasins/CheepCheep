#include "device_interlock.h"
#include "log.h"
#include "wiegand.h"
#include "client.h"

#include <string.h>

typedef struct {
    char id[ILOCK_SESS_ID_BYTES_MAX];
    int kwh; //": 0,
    card_t card; // ID card that started the interlock session
} interlock_session_t;

typedef struct {
    const config_general_t *config;
    const config_interlock_t *ilock_config;
    wieg_evt_handle_t evt_handle;
    interlock_session_t session;
} ilock_ctx_t;

static void interlock_handle_swipe(wieg_evt_t event, card_t *card, void *ctx);
static void ilock_end_session(ilock_ctx_t *sess);

static ilock_ctx_t _ctx;

status_t interlock_init(const config_t *config)
{
    memset(&_ctx.session, 0x00, sizeof(interlock_session_t));
    _ctx.config = config->general;
    _ctx.evt_handle = wieg_evt_handler_reg(WIEG_EVT_NEWCARD, interlock_handle_swipe, (void *)&_ctx);
    return -STATUS_UNIMPL;
}

static void interlock_handle_swipe(wieg_evt_t event, card_t *card, void *ctx)
{

}

static void ilock_end_session(ilock_ctx_t *sess)
{
    // Do we have a session right now, and is it unique from "system?"
    if (0 == strlen(sess->id) && 0 != strcmp("system", sess->id))
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

        ilock_power_control(_ctx.ilock_config, false);
        // TODO: RGB led state
    }
}

void ilock_power_control(config_interlock_t *config, bool on)
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
