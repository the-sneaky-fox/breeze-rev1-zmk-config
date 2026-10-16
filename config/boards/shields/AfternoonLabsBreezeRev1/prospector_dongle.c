#include <string.h>
#include <lvgl.h>
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>

#include <zmk/display.h>
#include <zmk/event_manager.h>
#include <zmk/hid_indicators.h>
#include <zmk/events/battery_state_changed.h>
#include <zmk/events/hid_indicators_changed.h>
#include <zmk/events/caps_word_state_changed.h>
#include <zmk/events/split_central_status_changed.h>
#include <dt-bindings/zmk/hid_indicators.h>

/* --- Swap peripheral 0 <-> 1 so left/right battery indicators match keyboard halves --- */

static int battery_swap_listener_cb(const zmk_event_t *eh) {
    struct zmk_peripheral_battery_state_changed *bat_ev =
        as_zmk_peripheral_battery_state_changed(eh);
    if (bat_ev != NULL && bat_ev->source < 2) {
        bat_ev->source = 1 - bat_ev->source;
        return ZMK_EV_EVENT_BUBBLE;
    }

    struct zmk_split_central_status_changed *conn_ev =
        as_zmk_split_central_status_changed(eh);
    if (conn_ev != NULL && conn_ev->slot < 2) {
        conn_ev->slot = 1 - conn_ev->slot;
        return ZMK_EV_EVENT_BUBBLE;
    }

    return ZMK_EV_EVENT_BUBBLE;
}

ZMK_LISTENER(prospector_battery_swap, battery_swap_listener_cb);
ZMK_SUBSCRIPTION(prospector_battery_swap, zmk_peripheral_battery_state_changed);
ZMK_SUBSCRIPTION(prospector_battery_swap, zmk_split_central_status_changed);

/* --- Caps Lock state -> Prospector modifier indicator ("CAPS" + highlight) --- */

static bool caps_lock_active = false;
static lv_obj_t *last_synced_screen = NULL;
static bool last_synced_caps = false;
static lv_timer_t *screen_sync_timer = NULL;

static void update_shift_labels_recursive(lv_obj_t *obj, const char *from, const char *to) {
    if (obj == NULL) {
        return;
    }
    if (lv_obj_check_type(obj, &lv_label_class)) {
        const char *txt = lv_label_get_text(obj);
        if (txt != NULL && strcmp(txt, from) == 0) {
            lv_label_set_text(obj, to);
        }
    }
    uint32_t count = lv_obj_get_child_count(obj);
    for (uint32_t i = 0; i < count; i++) {
        update_shift_labels_recursive(lv_obj_get_child(obj, i), from, to);
    }
}

static void sync_active_screen_caps_label(void) {
    lv_obj_t *scr = lv_screen_active();
    if (scr == NULL) {
        return;
    }
    if (caps_lock_active) {
        update_shift_labels_recursive(scr, "SHFT", "CAPS");
    } else {
        update_shift_labels_recursive(scr, "CAPS", "SHFT");
    }
}

static void screen_sync_timer_cb(lv_timer_t *timer) {
    ARG_UNUSED(timer);
    lv_obj_t *scr = lv_screen_active();
    if (scr != last_synced_screen || caps_lock_active != last_synced_caps) {
        sync_active_screen_caps_label();
        last_synced_screen = scr;
        last_synced_caps = caps_lock_active;
    }
}

static void caps_lock_update_cb(bool active) {
    caps_lock_active = active;
    if (screen_sync_timer == NULL) {
        screen_sync_timer = lv_timer_create(screen_sync_timer_cb, 100, NULL);
    }
    sync_active_screen_caps_label();
    last_synced_screen = lv_screen_active();
    last_synced_caps = active;
}

static bool caps_lock_get_state(const zmk_event_t *eh) {
    ARG_UNUSED(eh);
    bool active = (zmk_hid_indicators_get_current_profile() & HID_INDICATOR_CAPS_LOCK) != 0;
    raise_zmk_caps_word_state_changed((struct zmk_caps_word_state_changed){
        .active = active,
    });
    return active;
}

ZMK_DISPLAY_WIDGET_LISTENER(widget_prospector_caps_lock, bool,
                            caps_lock_update_cb, caps_lock_get_state)
ZMK_SUBSCRIPTION(widget_prospector_caps_lock, zmk_hid_indicators_changed);

/* --- Dual-pin D0/D1 CST816S touchscreen reset & IRQ support --- */

#if DT_NODE_HAS_STATUS(DT_NODELABEL(cst816s), okay)
#define CST816S_NODE DT_NODELABEL(cst816s)

struct cst816s_data_prefix {
    const struct device *dev;
    struct k_work work;
};

static const struct gpio_dt_spec touch_rst = GPIO_DT_SPEC_GET(CST816S_NODE, rst_gpios);
static const struct gpio_dt_spec touch_irq = GPIO_DT_SPEC_GET(CST816S_NODE, irq_gpios);
static struct gpio_callback touch_rst_cb;

static void touch_rst_isr(const struct device *port, struct gpio_callback *cb, uint32_t pins) {
    ARG_UNUSED(port);
    ARG_UNUSED(cb);
    ARG_UNUSED(pins);
    const struct device *dev = DEVICE_DT_GET(CST816S_NODE);
    struct cst816s_data_prefix *data = dev->data;
    k_work_submit(&data->work);
}

static int touch_pre_init(void) {
    if (gpio_is_ready_dt(&touch_irq)) {
        gpio_pin_configure_dt(&touch_irq, GPIO_OUTPUT_ACTIVE);
        k_msleep(5);
        gpio_pin_configure_dt(&touch_irq, GPIO_INPUT);
        k_msleep(50);
    }
    return 0;
}
SYS_INIT(touch_pre_init, POST_KERNEL, 85);

static int touch_post_init(void) {
    const struct device *dev = DEVICE_DT_GET(CST816S_NODE);
    if (!device_is_ready(dev) || !gpio_is_ready_dt(&touch_rst)) {
        return 0;
    }
    if (gpio_pin_configure_dt(&touch_rst, GPIO_INPUT) == 0 &&
        gpio_pin_interrupt_configure_dt(&touch_rst, GPIO_INT_EDGE_TO_ACTIVE) == 0) {
        gpio_init_callback(&touch_rst_cb, touch_rst_isr, BIT(touch_rst.pin));
        gpio_add_callback(touch_rst.port, &touch_rst_cb);
    }
    return 0;
}
SYS_INIT(touch_post_init, APPLICATION, 90);
#endif
