#include "modifier_indicator.h"

#include <zmk/display.h>
#ifdef CONFIG_DT_HAS_ZMK_BEHAVIOR_CAPS_WORD_ENABLED
#include <zmk/events/caps_word_state_changed.h>
#endif
#if IS_ENABLED(CONFIG_ZMK_HID_INDICATORS)
#include <zmk/hid_indicators.h>
#include <zmk/events/hid_indicators_changed.h>
#include <dt-bindings/zmk/hid_indicators.h>
#endif
#include <zmk/events/keycode_state_changed.h>
#include <zmk/event_manager.h>
#include <zmk/hid.h>

#include <fonts.h>
#include <modifier_order.h>
#include "display_colors.h"

static sys_slist_t widgets = SYS_SLIST_STATIC_INIT(&widgets);

struct modifier_indicator_state {
    bool mods[4];
    bool caps;
};

#ifdef CONFIG_DT_HAS_ZMK_BEHAVIOR_CAPS_WORD_ENABLED
static bool caps_word_active = false;
#endif

static void set_modifier_color(lv_obj_t *label, bool active) {
    lv_color_t color = active ? lv_color_hex(DISPLAY_COLOR_MOD_ACTIVE)
                               : lv_color_hex(DISPLAY_COLOR_MOD_INACTIVE);
    lv_obj_set_style_text_color(label, color, 0);
}

static void modifier_indicator_update_cb(struct modifier_indicator_state state) {
    struct zmk_widget_operator_modifier_indicator *widget;
    SYS_SLIST_FOR_EACH_CONTAINER(&widgets, widget, node) {
        for (int i = 0; i < 4; i++) {
            enum modifier_type type = modifier_order_get(i);
            if (type == MOD_TYPE_SHIFT) {
                if (state.caps) {
                    lv_label_set_text(widget->mod_labels[i], "CAPS");
                    lv_obj_set_style_text_color(widget->mod_labels[i],
                        lv_color_hex(DISPLAY_COLOR_MOD_CAPS_WORD), 0);
                    continue;
                } else {
                    lv_label_set_text(widget->mod_labels[i], modifier_order_get_text(i));
                }
            }
            set_modifier_color(widget->mod_labels[i], state.mods[type]);
        }
    }
}

static struct modifier_indicator_state modifier_indicator_get_state(const zmk_event_t *eh) {
#ifdef CONFIG_DT_HAS_ZMK_BEHAVIOR_CAPS_WORD_ENABLED
    if (eh != NULL) {
        const struct zmk_caps_word_state_changed *ev = as_zmk_caps_word_state_changed(eh);
        if (ev != NULL) {
            caps_word_active = ev->active;
        }
    }
#endif

    bool caps = false;
#ifdef CONFIG_DT_HAS_ZMK_BEHAVIOR_CAPS_WORD_ENABLED
    if (caps_word_active) {
        caps = true;
    }
#endif
#if IS_ENABLED(CONFIG_ZMK_HID_INDICATORS)
    if ((zmk_hid_indicators_get_current_profile() & HID_INDICATOR_CAPS_LOCK) != 0) {
        caps = true;
    }
#endif

    zmk_mod_flags_t mods = zmk_hid_get_explicit_mods();

    struct modifier_indicator_state state = {
        .mods = {false, false, false, false},
        .caps = caps,
    };

    state.mods[MOD_TYPE_GUI] = (mods & (MOD_LGUI | MOD_RGUI)) != 0;
    state.mods[MOD_TYPE_ALT] = (mods & (MOD_LALT | MOD_RALT)) != 0;
    state.mods[MOD_TYPE_CTRL] = (mods & (MOD_LCTL | MOD_RCTL)) != 0;
    state.mods[MOD_TYPE_SHIFT] = (mods & (MOD_LSFT | MOD_RSFT)) != 0;

    return state;
}

ZMK_DISPLAY_WIDGET_LISTENER(widget_operator_modifier_indicator, struct modifier_indicator_state,
                            modifier_indicator_update_cb, modifier_indicator_get_state)
ZMK_SUBSCRIPTION(widget_operator_modifier_indicator, zmk_keycode_state_changed);

#ifdef CONFIG_DT_HAS_ZMK_BEHAVIOR_CAPS_WORD_ENABLED
ZMK_SUBSCRIPTION(widget_operator_modifier_indicator, zmk_caps_word_state_changed);
#endif

#if IS_ENABLED(CONFIG_ZMK_HID_INDICATORS)
ZMK_SUBSCRIPTION(widget_operator_modifier_indicator, zmk_hid_indicators_changed);
#endif

static lv_obj_t *create_separator(lv_obj_t *parent) {
    lv_obj_t *sep = lv_obj_create(parent);
    lv_obj_set_size(sep, 2, 24);
    lv_obj_set_style_bg_color(sep, lv_color_hex(DISPLAY_COLOR_MOD_SEPARATOR), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(sep, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(sep, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(sep, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(sep, 0, LV_PART_MAIN);
    return sep;
}

static lv_obj_t *create_mod_label(lv_obj_t *parent, const char *text) {
    lv_obj_t *label = lv_label_create(parent);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_font(label, &FG_Medium_20, LV_PART_MAIN);
    lv_obj_set_style_text_color(label, lv_color_hex(DISPLAY_COLOR_MOD_INACTIVE), LV_PART_MAIN);
    return label;
}

int zmk_widget_operator_modifier_indicator_init(struct zmk_widget_operator_modifier_indicator *widget, lv_obj_t *parent) {
    widget->obj = lv_obj_create(parent);
    lv_obj_set_size(widget->obj, 230, 24);
    lv_obj_set_style_bg_opa(widget->obj, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(widget->obj, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(widget->obj, 0, LV_PART_MAIN);

    lv_obj_set_flex_flow(widget->obj, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(widget->obj, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    for (int i = 0; i < 4; i++) {
        widget->mod_labels[i] = create_mod_label(widget->obj, modifier_order_get_text(i));
        if (i < 3) {
            create_separator(widget->obj);
        }
    }

    sys_slist_append(&widgets, &widget->node);
    widget_operator_modifier_indicator_init();

    return 0;
}

lv_obj_t *zmk_widget_operator_modifier_indicator_obj(struct zmk_widget_operator_modifier_indicator *widget) {
    return widget->obj;
}
