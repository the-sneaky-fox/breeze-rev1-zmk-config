#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/drivers/pwm.h>
#include <zephyr/drivers/led.h>
#include <zephyr/sys/printk.h>
#include <zephyr/sys/atomic.h>

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(als, 4);

#include <zephyr/settings/settings.h>
#include <prospector_brightness.h>

static const struct device *pwm_leds_dev = DEVICE_DT_GET_ONE(pwm_leds);
#define DISP_BL DT_NODE_CHILD_IDX(DT_NODELABEL(disp_bl))

/*
 * current_brightness is accessed from both the ALS thread (when the ambient
 * light sensor is populated) and the display work queue (via swipe-gesture
 * brightness control when PROSPECTOR_TOUCHSCREEN is enabled). Use atomic_t
 * so individual reads and writes are race-free; last writer wins, which is
 * acceptable for a non-critical display setting.
 */
static atomic_t current_brightness =
#if IS_ENABLED(CONFIG_PROSPECTOR_USE_AMBIENT_LIGHT_SENSOR)
    100
#else
    CONFIG_PROSPECTOR_FIXED_BRIGHTNESS
#endif
    ;

#if IS_ENABLED(CONFIG_SETTINGS)

#define PROSPECTOR_BRIGHTNESS_SAVE_DEBOUNCE 1000

static struct k_work_delayable brightness_save_work;

static void brightness_save_work_handler(struct k_work *work) {
    ARG_UNUSED(work);
    uint8_t b = (uint8_t)atomic_get(&current_brightness);
    settings_save_one("prospector/brightness", &b, sizeof(b));
}

static int brightness_settings_load_cb(const char *name, size_t len, settings_read_cb read_cb,
                                       void *cb_arg) {
    const char *next;
    if (settings_name_steq(name, "brightness", &next) && !next) {
        if (len != sizeof(uint8_t)) {
            return -EINVAL;
        }

        uint8_t b;
        int rc = read_cb(cb_arg, &b, sizeof(b));
        if (rc >= 0) {
            if (b < 1) {
                b = 1;
            } else if (b > 100) {
                b = 100;
            }
            atomic_set(&current_brightness, (atomic_val_t)b);
            led_set_brightness(pwm_leds_dev, DISP_BL, b);
        }

        return MIN(rc, 0);
    }
    return -ENOENT;
}

SETTINGS_STATIC_HANDLER_DEFINE(prospector_brightness, "prospector", NULL,
                               brightness_settings_load_cb, NULL, NULL);

static int brightness_settings_init(const struct device *dev) {
    ARG_UNUSED(dev);
    k_work_init_delayable(&brightness_save_work, brightness_save_work_handler);
    return 0;
}

SYS_INIT(brightness_settings_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);

#endif /* IS_ENABLED(CONFIG_SETTINGS) */

uint8_t prospector_brightness_step(int8_t delta) {
    int16_t b = (int16_t)atomic_get(&current_brightness) + delta;
    if (b < 1) {
        b = 1;
    } else if (b > 100) {
        b = 100;
    }
    atomic_set(&current_brightness, (atomic_val_t)b);
    if (led_set_brightness(pwm_leds_dev, DISP_BL, (uint8_t)b)) {
        LOG_ERR("Failed to set brightness");
    }
#if IS_ENABLED(CONFIG_SETTINGS)
    k_work_reschedule(&brightness_save_work, K_MSEC(PROSPECTOR_BRIGHTNESS_SAVE_DEBOUNCE));
#endif
    return (uint8_t)b;
}

uint8_t prospector_brightness_get(void) {
    return (uint8_t)atomic_get(&current_brightness);
}

#if IS_ENABLED(CONFIG_PROSPECTOR_USE_AMBIENT_LIGHT_SENSOR)

#define SENSOR_MIN      0       // Minimum sensor reading
#define SENSOR_MAX      100   // Maximum sensor reading
#define PWM_MIN         1       // Minimum PWM duty cycle (%) - keep display visible
#define PWM_MAX         100     // Maximum PWM duty cycle (%)

#define FADE_STEP                        1
#define FADE_SLEEP_BRIGHTEN_MS           3
#define FADE_SLEEP_DARKEN_MS             10
#define FADE_THRESHOLD                   10

#define NORMAL_SAMPLE_SLEEP_MS           100

#define BURST_SAMPLE_SLEEP_MS            30
#define BURST_SAMPLE_TIMEOUT             10
#define BURST_SAMPLE_CONSECUTIVE         3

uint8_t map_light_to_pwm(int32_t sensor_reading) {
    // Handle invalid/error readings
    if (sensor_reading < SENSOR_MIN) {
        return PWM_MIN;  // Default to minimum brightness on error
    }

    // Clamp to maximum
    if (sensor_reading > SENSOR_MAX) {
        sensor_reading = SENSOR_MAX;
    }

    // Linear mapping
    uint8_t pwm_value = (uint8_t)(
        PWM_MIN + ((PWM_MAX - PWM_MIN) *
        (sensor_reading - SENSOR_MIN)) / (SENSOR_MAX - SENSOR_MIN)
    );

    return pwm_value;
}

uint8_t bl_fade(uint8_t source, uint8_t target) {
    bool increasing = target > source;
    int b = source;

    while ((increasing && b < target) || (!increasing && b > target)) {
        if (led_set_brightness(pwm_leds_dev, DISP_BL, (uint8_t)b)) {
            LOG_ERR("Failed to set brightness");
        }

        b += increasing ? FADE_STEP : -FADE_STEP;

        // Ensure we don't overshoot bounds
        if (b > 100) {
            b = 100;
        } else if (b < 0) {
            b = 0;
        }

        atomic_set(&current_brightness, (atomic_val_t)b);
        k_msleep(increasing ? FADE_SLEEP_BRIGHTEN_MS : FADE_SLEEP_DARKEN_MS);
    }

    return 0;
}

extern void als_thread(void *d0, void *d1, void *d2) {
    ARG_UNUSED(d0);
    ARG_UNUSED(d1);
    ARG_UNUSED(d2);

    const struct device *dev;
    struct sensor_value intensity;
    uint8_t mapped_brightness;

    dev = DEVICE_DT_GET_ONE(avago_apds9960);
    if (!device_is_ready(dev)) {
        printk("sensor: device not ready.\n");
    }

    while (1) {

        k_msleep(NORMAL_SAMPLE_SLEEP_MS);

        if (sensor_sample_fetch(dev)) {
            LOG_ERR("sensor_sample fetch failed\n");
        }

        if (sensor_channel_get(dev, SENSOR_CHAN_LIGHT, &intensity)) {
            LOG_ERR("Cannot read ALS data.\n");
        }

        mapped_brightness = map_light_to_pwm(intensity.val1);

        if (abs(mapped_brightness - atomic_get(&current_brightness)) > FADE_THRESHOLD) {
            uint8_t integrator = 0;

            for (int i = 0; i < BURST_SAMPLE_TIMEOUT; i++) {
                k_msleep(BURST_SAMPLE_SLEEP_MS);

                if (sensor_sample_fetch(dev)) {
                    LOG_ERR("sensor_sample fetch failed\n");
                }
                if (sensor_channel_get(dev, SENSOR_CHAN_LIGHT, &intensity)) {
                    LOG_ERR("Cannot read ALS data.\n");
                }

                mapped_brightness = map_light_to_pwm(intensity.val1);

                if (abs(mapped_brightness - atomic_get(&current_brightness)) > FADE_THRESHOLD) {
                    integrator++;
                    if (integrator >= BURST_SAMPLE_CONSECUTIVE) {
                        bl_fade((uint8_t)atomic_get(&current_brightness), mapped_brightness);
                        atomic_set(&current_brightness, (atomic_val_t)mapped_brightness);
                        break;
                    }
                }
            }
        }
    }
}

K_THREAD_DEFINE(als_tid, 1024, als_thread, NULL, NULL, NULL, K_LOWEST_APPLICATION_THREAD_PRIO, 0,
                0);

#else

static int init_fixed_brightness(void) {
    led_set_brightness(pwm_leds_dev, DISP_BL, (uint8_t)atomic_get(&current_brightness));

    return 0;
}

SYS_INIT(init_fixed_brightness, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);

#endif
