#include "tsync.h"
#include "pico/multicore.h"
#include "hardware/watchdog.h"
#include "hardware/rtc.h"

#define RESET_SLEEP_TIME 500

// published to Python API
GLOBAL_FLAG(is_primary);
GLOBAL_FLAG_RO(core1_running);
GLOBAL_FLAG(core1_cancel);
GLOBAL_FLAG(manual_hand_pos);
GLOBAL_FLAG(manual_pwm_raw);
GLOBAL_FLAG(manual_pwm_duty);
GLOBAL_RO_INT(rx_errors);
GLOBAL_RO_INT(frame_rate);

// internal state flag
static bool should_reset = false;

// frame counter
static uint32_t current_frame_s = 0;
static uint current_frame_count = 0;

static void count_frame(void)
{
    uint32_t now_s = rtc_hw->rtc_0;
    if (now_s != current_frame_s) {
        current_frame_s = now_s;
        frame_rate = current_frame_count;
        current_frame_count = 1;
    } else {
        current_frame_count += 1;
    }
}

static void core1_cleanup(void)
{
    if (should_reset) {
        sleep_ms(RESET_SLEEP_TIME);
        watchdog_reboot(0, SRAM_END, 0);
        for (;;) {
            __wfi();
        }
    }
    core1_running = false;
}

NO_ARG_MP_FUNCTION(draw_loop_primary, {
    // update time from RTC/ticks_us
    update_time();

    if (!manual_hand_pos) {
        // update hand_pos from time
        update_hand_pos();
    }

    if (!manual_pwm_raw) {
        // update pwm_raw from hand_pos
        update_pwm_raw_from_hands(DEFAULT_HAND_FLAGS);
    }

    if (!manual_pwm_duty) {
        // update pwm_duty from pwm_raw
        update_pwm_duty();
    }

    // update i2c_buffer from pwm_duty
    update_i2c_tx_buffer();
    // send i2c_buffer to secondary over UART
    i2c_tx_send();
    // apply pwm_duty to our bank of PWMs
    paint_pwm();
    // increment frame counter
    count_frame();
})

static void core1_primary(void)
{
    setup_pwm();
    while (!core1_cancel)
    {
        draw_loop_primary();
    }
    core1_cleanup();
}

NO_ARG_MP_FUNCTION(draw_loop_secondary, {
    // update time from RTC/ticks_us
    update_time();
    // update pwm_duty from UART
    update_i2c_rx_buffer();
    // apply pwm_duty to our bank of PWMs
    paint_pwm();
    // increment frame counter
    count_frame();
})

static void core1_secondary(void)
{
    setup_pwm();
    while (!core1_cancel)
    {
        draw_loop_secondary();
    }
    core1_cleanup();
}

NO_ARG_MP_FUNCTION(core1_init, {
    if (core1_running) return;

    if (get_core_num() != 0)
        mp_raise_ValueError("Can only init from core 0");

    // start second thread
    if (is_primary)
        multicore_launch_core1(core1_primary);
    else
        multicore_launch_core1(core1_secondary);
    core1_running = true;
})

NO_ARG_MP_FUNCTION(core1_reset_soon, {
    if (!core1_running) {
        return;
    }
    should_reset = true;
    core1_cancel = true;
    return;
})