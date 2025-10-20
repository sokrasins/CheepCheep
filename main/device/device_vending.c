#include "device_vending.h"
#include "wiegand.h"
#include "client.h"

typedef struct
{
    const config_general_t *config;
    wieg_evt_handle_t       evt_handle;
} vending_ctx_t;

static status_t vending_init(const config_t *config);
static status_t vending_deinit(void);
static void     vending_handle_swipe(wieg_evt_t event, card_t *card, void *ctx);
static status_t client_cmd_handler(msg_t *msg);

static vending_ctx_t _ctx;

const device_t vending = {
    .init   = vending_init,
    .deinit = vending_deinit,
};

static status_t
vending_init (const config_t *config)
{
    _ctx.evt_handle = wieg_evt_handler_reg(
        WIEG_EVT_NEWCARD, vending_handle_swipe, (void *)&_ctx);

    // Register cb for server requests
    client_handler_register(client_cmd_handler);

    return -STATUS_UNIMPL;
}

static status_t
vending_deinit (void)
{
    return STATUS_OK;
}

static void
vending_handle_swipe (wieg_evt_t event, card_t *card, void *ctx)
{
}

static status_t
client_cmd_handler (msg_t *msg)
{
    status_t status = -STATUS_UNAVAILABLE;
    return status;
}