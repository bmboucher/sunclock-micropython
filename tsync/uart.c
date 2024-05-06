#include "tsync.h"
#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "uart.pio.h"
#include "pico/multicore.h"
#include "hardware/watchdog.h"
#include "hardware/rtc.h"
#include <string.h>

#define START_WORD 1<<15
#define SERIAL_BAUD 115200
#define UART_PIN 27
#define UART_PIO pio0
#define UART_SM 0
#define RESET_SLEEP_TIME 500

static bool uart_configured = false;
static void init_uart_tx() {
    uint offset_tx = pio_add_program(UART_PIO, &uart_tx_program);
    uart_tx_program_init(UART_PIO, UART_SM, offset_tx, UART_PIN, SERIAL_BAUD);
    uart_buffer[0] = START_WORD;
}

static void init_uart_rx() {
    uint offset_rx = pio_add_program(UART_PIO, &uart_rx_program);
    uart_rx_program_init(UART_PIO, UART_SM, offset_rx, UART_PIN, SERIAL_BAUD);
}

static void init_uart() {
    if (uart_configured) return;
    if (is_primary) {
        init_uart_tx();
    } else {
        init_uart_rx();
    }
    uart_configured = true;
}

NO_ARG_MP_FUNCTION(update_uart_tx_buffer, {
    if (!is_primary) return;   // only tx from primary

    uint16_t checksum = 0;
    for (uint i = 0; i < N_TUBES_HALF; i++) {
        const uint8_t tube = tube_map[i + N_TUBES_HALF];
        const uint16_t duty_u16 = pwm_duty[tube];
        uart_buffer[i + 1] = duty_u16;
        checksum += duty_u16;
    }
    uart_buffer[N_TUBES_HALF + 1] = checksum;
});

NO_ARG_MP_FUNCTION(update_uart_rx_buffer, {
    if (is_primary) return;   // only rx on secondary
    if (!uart_configured) init_uart();
    uint index = 0;
    uint16_t checksum = 0;
    while (!core1_cancel) {
        const uint16_t read_value = uart_rx_program_get(UART_PIO, UART_SM, &core1_cancel);
        if (read_value & START_WORD) {
            index = 0;
            checksum = 0;
        } else {
            index += 1;
        }
        uart_buffer[index] = read_value;
        if (index == N_TUBES_HALF + 1) {
            if (checksum == read_value) {
                for (uint i = 0; i < N_TUBES_HALF; i++) {
                    const uint8_t tube = tube_map[i + N_TUBES_HALF];
                    pwm_duty[tube] = uart_buffer[i + 1];
                }
                break;
            }
            rx_errors += 1;          // otherwise, log the frame error
            index = 0; checksum = 0; // reset state, keep looping
        } else if (index > 0) {
            checksum += read_value;
        }
    }
});

NO_ARG_MP_FUNCTION(uart_tx_send, {
    if (!is_primary) return;   // only tx from primary
    if (!uart_configured) init_uart();

    for (uint i = 0; i < N_TUBES_HALF + 2; i++) {
        uart_tx_program_put(UART_PIO, UART_SM, uart_buffer[i]);
    }
});