#include "tsync.h"
#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "i2c.pio.h"
#include "pico/multicore.h"
#include "hardware/watchdog.h"
#include "hardware/rtc.h"
#include <string.h>

#define START_WORD (1U<<15)

static bool i2c_configured = false;

static void init_i2c() {
    if (i2c_configured) return;
    if (is_primary) {
        i2c_tx_program_init();
    } else {
        i2c_rx_program_init();
    }
    i2c_configured = true;
}

NO_ARG_MP_FUNCTION(update_i2c_tx_buffer, {
    if (!is_primary) return;   // only tx from primary

    i2c_buffer[0] = START_WORD + pwm_bits;
    uint16_t checksum = 0;
    for (uint i = 0; i < N_TUBES_HALF; i++) {
        const uint8_t tube = tube_map[i + N_TUBES_HALF];
        const uint16_t duty_u16 = pwm_duty[tube];
        i2c_buffer[i + 1] = duty_u16;
        checksum += duty_u16;
    }
    i2c_buffer[N_TUBES_HALF + 1] = checksum & ~START_WORD;
});

NO_ARG_MP_FUNCTION(update_i2c_rx_buffer, {
    if (is_primary) return;   // only rx on secondary
    if (!i2c_configured) init_i2c();
    uint index = 0;
    uint16_t checksum = 0;
    bool saw_start = false;
    uint8_t rx_pwm_bits = 0;
    while (!core1_cancel) {
        //const uint16_t read_value = (i2c_rx_program_get(UART_PIO, UART_SM, &core1_cancel) >> 16);
        const uint16_t read_value = i2c_rx_program_get(&core1_cancel);
        if (read_value & START_WORD) {
            index = 0;
            checksum = 0;
            saw_start = true;
            rx_pwm_bits = (uint8_t)(read_value & ~START_WORD);
        } else if (!saw_start) {
            rx_errors += 1;
            continue;
        } else {
            index += 1;
        }
        i2c_buffer[index] = read_value;
        if (index == N_TUBES_HALF + 1) {
            if ((checksum & ~START_WORD) == read_value) {
                if (rx_pwm_bits <= 15) {
                    pwm_bits = rx_pwm_bits; // will be configured on next paint
                }
                for (uint i = 0; i < N_TUBES_HALF; i++) {
                    const uint8_t tube = tube_map[i + N_TUBES_HALF];
                    pwm_duty[tube] = i2c_buffer[i + 1];
                }
                return;
            } else {
                rx_errors += 1;          // otherwise, log the frame error
                index = 0; checksum = 0; // reset state, keep looping
            }
        } else if (index > 0) {
            checksum += read_value;
        }
    }
});

NO_ARG_MP_FUNCTION(i2c_tx_send, {
    if (!is_primary) return;   // only tx from primary
    if (!i2c_configured) init_i2c();

    for (uint i = 0; i < N_TUBES_HALF + 2; i++) {
        i2c_tx_program_put(i2c_buffer[i]);
        //i2c_tx_program_put(UART_PIO, UART_SM, i2c_buffer[i]);
    }
});