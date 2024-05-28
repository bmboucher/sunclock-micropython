#ifndef __TSYNC_H
#define __TSYNC_H

// Include MicroPython API.
#include "py/runtime.h"
#include "tsync_macros.h"  // GLOBAL_FLAG_* and 

/****** CONSTANTS *******/

// all fixed-point values are assumed to be 16.16 unless otherwise noted
// i.e. to multiply 16.16 a by 16.16 b we use ((a*b)>>16)

#define FP_1_0  (1<<16)
#define FP_0_5  (1<<15)
#define FP_0_25 (1<<14)

// number of tubes handled by each pico
#define N_TUBES_HALF 12

// number of tubes total
#define N_TUBES (N_TUBES_HALF*2)

// number of hands (hour/minute/second)
#define N_HANDS 3

// assume e^(-x^2) is 0 outside this many std devs
#define GAUSS_LIMIT 4
#define GAUSS_LIMIT_UINT (GAUSS_LIMIT<<16)

/****** GLOBAL VARIABLES *******/

GLOBAL_FLAG_DECL(is_primary);
GLOBAL_FLAG_RO_DECL(core1_running);
GLOBAL_FLAG_DECL(core1_cancel);
GLOBAL_FLAG_DECL(manual_hand_pos);
GLOBAL_FLAG_DECL(manual_pwm_raw);
GLOBAL_FLAG_DECL(manual_pwm_duty);
GLOBAL_RO_INT_DECL(rx_errors);
GLOBAL_RO_INT_DECL(frame_rate);

/****** BUFFERS *******/
// Defined in buffers.c - these are shared with the Python runtime through
// memoryviews so that most function calls can be 0 arg.

// last clock time read
extern uint16_t time[7]; // y,m,d,h,m,s,ms
extern int32_t time_adj[4]; // h,m,s,ms

// "hands" are gaussian distributions A*exp(-(k*x)^2) where A,k may vary by hand
extern uint32_t hand_k[N_HANDS];
extern uint32_t hand_A[N_HANDS];

// hand positions (second, minute, hour) as a fraction 0.32 [0,1) of a circle
extern uint16_t hand_pos[N_HANDS];

// 1.31 [0,1] fixed-point, calculated from gaussians or populated from Python
extern uint32_t pwm_raw[N_TUBES];

// calibration points for pwm_raw = 0 and pwm_raw = 1
extern uint16_t pwm_lo[N_TUBES];
extern uint16_t pwm_hi[N_TUBES];

// actual pwm duty = pwm_lo + pwm_raw*(pwm_hi - pwm_lo)
extern uint16_t pwm_duty[N_TUBES];

// mapping of tubes to pins
// both are mapped to "slots" since pin numbers have a gap between 22 and 26
// slots 0-11 are the primary, 12-23 is the secondary
extern uint8_t tube_map[N_TUBES];
extern uint8_t pin_map[N_TUBES];  // {0..22, 26}

// send/receive buffer for UART
extern uint16_t i2c_buffer[N_TUBES_HALF + 2]; // start data[0] ... data[11] checksum

// Python methods tsync.get_<buffer>() -> memoryview
MP_DECLARE_CONST_FUN_OBJ_0(tsync_get_time_obj);
MP_DECLARE_CONST_FUN_OBJ_0(tsync_get_time_adj_obj);
MP_DECLARE_CONST_FUN_OBJ_0(tsync_get_hand_k_obj);
MP_DECLARE_CONST_FUN_OBJ_0(tsync_get_hand_A_obj);
MP_DECLARE_CONST_FUN_OBJ_0(tsync_get_hand_pos_obj);
MP_DECLARE_CONST_FUN_OBJ_0(tsync_get_pwm_raw_obj);
MP_DECLARE_CONST_FUN_OBJ_0(tsync_get_pwm_lo_obj);
MP_DECLARE_CONST_FUN_OBJ_0(tsync_get_pwm_hi_obj);
MP_DECLARE_CONST_FUN_OBJ_0(tsync_get_pwm_duty_obj);
MP_DECLARE_CONST_FUN_OBJ_0(tsync_get_tube_map_obj);
MP_DECLARE_CONST_FUN_OBJ_0(tsync_get_pin_map_obj);
MP_DECLARE_CONST_FUN_OBJ_0(tsync_get_i2c_buffer_obj);

/****** CLOCK *******/

// updates the values in time from the rtc and ticks_us
NO_ARG_MP_FUNCTION_DECL(update_time);

// updates hand_pos based on time
NO_ARG_MP_FUNCTION_DECL(update_hand_pos);

// tsync.epoch_millis(local: bool = True) -> int
MP_DECLARE_CONST_FUN_OBJ_VAR_BETWEEN(tsync_epoch_millis_obj);

// tsync.epoch_seconds_float(local: bool = True) -> float
MP_DECLARE_CONST_FUN_OBJ_VAR_BETWEEN(tsync_epoch_seconds_float_obj);

// tsync.localtime() -> Tuple[int,int,int,int,int,int,int]
MP_DECLARE_CONST_FUN_OBJ_0(tsync_localtime_obj);

// tsync.get_dst_info() -> Tuple[bool,int,int]
MP_DECLARE_CONST_FUN_OBJ_0(tsync_get_dst_info_obj);

/****** HANDS *******/

// calculate e^(-input) in 16.16 fixed-point
uint32_t fixed_point_exp(uint32_t input);
// tsync.fixed_point_exp(input: int|float) -> int|float
MP_DECLARE_CONST_FUN_OBJ_1(tsync_fixed_point_exp_obj);

// calculate e^(-input^2) in 16.16 fixed-point
uint32_t fixed_point_gaussian(uint32_t input);
// tsync.fixed_point_gaussian(input: int|float) -> int|float
MP_DECLARE_CONST_FUN_OBJ_1(tsync_fixed_point_gaussian_obj);

#define DEFAULT_HAND_FLAGS 0b00000111

// updates pwm_raw from hand_pos/hand_A/hand_k using gaussians for hands
// hand_flags = .....HMS used to select hands (i.e. default 7 = all hands)
void update_pwm_raw_from_hands(uint8_t hand_flags);
// tsync.update_pwm_raw_from_hands(hand_flags: int = 7) -> None
MP_DECLARE_CONST_FUN_OBJ_VAR_BETWEEN(tsync_update_pwm_raw_from_hands_obj);

// updates pwm_raw from a single guassian defined explicitly
void update_pwm_raw_from_single_gaussian(uint16_t pos, uint32_t k, uint32_t A);
// tsync.update_pwm_raw_from_single_gaussian(pos: int, k: int, A: int) -> None
MP_DECLARE_CONST_FUN_OBJ_3(tsync_update_pwm_raw_from_single_gaussian_obj);

/****** UART *******/

// updates i2c_buffer from pwm_duty (only on primary)
NO_ARG_MP_FUNCTION_DECL(update_i2c_tx_buffer);

// updates i2c_buffer from the UART - will block until a value frame is received
NO_ARG_MP_FUNCTION_DECL(update_i2c_rx_buffer);

// sends the contents of i2c_buffer via UART
NO_ARG_MP_FUNCTION_DECL(i2c_tx_send);

/****** PWM *******/

// pwm_bits is the bit accuracy of the generated PWM
// more bits -> lower frequency, default is 11 bits ~= 60kHz
// allow this to be configured from Python (must match b/w primary and secondary)

extern uint8_t pwm_bits;

// tsync.get_pwm_bits() -> int
MP_DECLARE_CONST_FUN_OBJ_0(tsync_get_pwm_bits_obj);
// tsync.set_pwm_bits(pwm_bits: int) -> None
MP_DECLARE_CONST_FUN_OBJ_1(tsync_set_pwm_bits_obj);

// updates the pwm_duty buffer from pwm_raw/pwm_lo/pwm_hi/pwm_bits
NO_ARG_MP_FUNCTION_DECL(update_pwm_duty);

// applies (half) the values in pwm_duty to the pwm outputs
NO_ARG_MP_FUNCTION_DECL(paint_pwm);

void setup_pwm();

/****** CORE1 *******/

NO_ARG_MP_FUNCTION_DECL(draw_loop_primary);

NO_ARG_MP_FUNCTION_DECL(draw_loop_secondary);

NO_ARG_MP_FUNCTION_DECL(core1_init);

NO_ARG_MP_FUNCTION_DECL(core1_reset_soon);

#endif //__TSYNC_H