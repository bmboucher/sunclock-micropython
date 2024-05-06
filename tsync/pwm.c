#include "tsync.h"

#include "pico/stdlib.h"
#include "hardware/pwm.h"
#include "hardware/gpio.h"

// If we run the PWM with the fastest clock (clk_div=1) and wrap at the maximum
// 16-bit value, we get a PWM frequency of 125MHz/65536 ~= 2kHz. This might be
// too low so we reduce the PWM accuracy to pwm_bits < 16; with the default of
// 11-bits we get a PWM frequency of ~61kHz. This also allows the UART to be
// defined as 16n1 and we can use the leading bits for other things.
#define DEFAULT_PWM_BITS 11
static uint8_t pwm_bits = DEFAULT_PWM_BITS;
static uint16_t pwm_mask = (1 << (1 + DEFAULT_PWM_BITS)) - 1;

static bool pwm_configured = false;
static void setup_pwm() {
    // set all pwm slices to the same frequency
    const uint16_t wrap = 1 << pwm_bits;
    for (uint slice = 0; slice < NUM_PWM_SLICES; slice++) {
        pwm_set_clkdiv_int_frac(slice, 1, 0);
        pwm_set_wrap(slice, wrap);
    }

    // set 12 pins to output from pwm and 12 to high-impedance
    for (uint i = 0; i < N_TUBES_HALF; i++) {
        const uint active = is_primary ? i : i + N_TUBES_HALF;
        const uint disabled = is_primary ? i + N_TUBES_HALF : i;
        gpio_set_function(pin_map[active], GPIO_FUNC_PWM);
        pwm_set_gpio_level(pin_map[active], 0);
        gpio_init(pin_map[disabled]);
        gpio_set_dir(pin_map[disabled], GPIO_IN);
    }
    pwm_configured = true;
}

// tsync.get_pwm_bits() -> int
static mp_obj_t tsync_get_pwm_bits(void) {
    return mp_obj_new_int(pwm_bits);
}
MP_DEFINE_CONST_FUN_OBJ_0(tsync_get_pwm_bits_obj, tsync_get_pwm_bits);

// tsync.set_pwm_bits(pwm_bits: int) -> None
static mp_obj_t tsync_set_pwm_bits(mp_obj_t pwm_bits_obj) {
    if (!mp_obj_is_int(pwm_bits_obj)) {
        mp_raise_TypeError("pwm_bits must be an integer");
    }
    int pwm_bits_in = mp_obj_get_int(pwm_bits_obj);
    if (pwm_bits_in < 2 || pwm_bits_in > 15) {
        mp_raise_ValueError("pwm_bits must be in range 2..15");
    }
    pwm_bits = pwm_bits_in;
    pwm_mask = (1 << (1 + pwm_bits)) - 1;
    setup_pwm();
    return mp_const_none;
}
MP_DEFINE_CONST_FUN_OBJ_1(tsync_set_pwm_bits_obj, tsync_set_pwm_bits);

// interpolates pwm_duty from pwm_raw/pwm_lo/pwm_hi
NO_ARG_MP_FUNCTION(update_pwm_duty, {
    for (uint i = 0; i < N_TUBES; i++) {
        const uint8_t tube = tube_map[i];

        const uint32_t raw = pwm_raw[tube];                  // 16.16 [0,1]
        const uint32_t lo = (uint32_t)pwm_lo[tube];          // 32.0 [0,2^16)
        const uint32_t diff = (uint32_t)pwm_hi[tube] - lo;   // 32.0 [0,2^16)
        // 16.16 [0,1] x 32.0 [0,2^16) = 16.16 [0,2^16) -> rescale to 16.0 [0,2^16)
        const uint16_t interp = (uint16_t)(lo + ((diff * raw) >> 16));
        pwm_duty[tube] = interp >> (16 - pwm_bits);
    }
})

NO_ARG_MP_FUNCTION(paint_pwm, {
    if (!pwm_configured) setup_pwm();
    for (uint i = 0; i < N_TUBES_HALF; i++) {
        const uint slot = is_primary ? i : i + N_TUBES_HALF;
        const uint pin = pin_map[slot];
        const uint tube = tube_map[slot];
        const uint16_t duty_u16 = pwm_duty[tube];

        pwm_set_gpio_level(pin, duty_u16);
    }
})