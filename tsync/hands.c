#include "tsync.h"
#include <string.h>

// all fixed-point values are assumed to be 16.16 unless otherwise noted
// i.e. to multiply 16.16 a by 16.16 b we use ((a*b)>>16)

// assume e^(-x^2) is 0 outside this many std devs
#define GAUSS_LIMIT 4
#define GAUSS_LIMIT_UINT (GAUSS_LIMIT<<16)

#define FP_1_0 (1<<16)
#define FP_0_25 (1<<14)

// calculate e^(-input)
uint32_t fixed_point_exp(uint32_t input)
{
    // divide the input by (2^scale) to get it in the [0,0.25) range
    uint scale = 0;
    while (input >= FP_0_25) {
        input = input >> 1;
        scale += 1;
    }

    // taylor series: e^-x ~= 1 - x + x^2/2
    // note that we fold the /2 into the bitshift for the multiplication
    uint32_t result = FP_1_0 - input + ((input*input)>>17);

    // now we undo the scaling by calculating result^(2^scale)
    for (uint i = 0; i < scale; i++) {
        result = (result * result) >> 16;
    }
    return result;
}

// calculate e^(-input^2)
uint32_t fixed_point_gaussian(uint32_t input)
{
    if (input >= GAUSS_LIMIT_UINT) return 0;
    const uint32_t input_sq = (input * input) >> 16;
    return fixed_point_exp(input_sq);
}

static mp_obj_t call_fixed_point(mp_obj_t input_obj, uint32_t (*func)(uint32_t)) {
    if (mp_obj_is_int(input_obj)) {
        const uint32_t input = (uint32_t)mp_obj_get_int(input_obj);
        return mp_obj_new_int(func(input));
    } else if (mp_obj_is_float(input_obj)) {
        const mp_float_t input = mp_obj_get_float(input_obj);
        const uint32_t input_fixed = (uint32_t)(input * (1<<16));
        const uint32_t output_fixed = func(input_fixed);
        const mp_float_t output = (mp_float_t)output_fixed / (1<<16);
        return mp_obj_new_float(output);
    } else {
        mp_raise_TypeError("Expected an int or float");
    }
}

mp_obj_t tsync_fixed_point_exp(mp_obj_t input_obj) {
    return call_fixed_point(input_obj, fixed_point_exp);
}
MP_DEFINE_CONST_FUN_OBJ_1(tsync_fixed_point_exp_obj, tsync_fixed_point_exp);

mp_obj_t tsync_fixed_point_gaussian(mp_obj_t input_obj) {
    return call_fixed_point(input_obj, fixed_point_gaussian);
}
MP_DEFINE_CONST_FUN_OBJ_1(tsync_fixed_point_gaussian_obj, tsync_fixed_point_gaussian);


static void update_single_hand(uint16_t pos, uint32_t k, uint32_t A)
{
    const uint32_t x_hand = (uint32_t)pos * N_TUBES;
    for (uint32_t tube = 0; tube < N_TUBES; tube++) {
        // calcualte x_tube = k*abs(tube - hand)
        uint32_t x_tube = tube << 16;
        x_tube = x_tube > x_hand ? x_tube - x_hand : x_hand - x_tube;
        x_tube = (x_tube * k) >> 16;

        // will return 0 from fixed_point_exp anyway
        if (x_tube >= GAUSS_LIMIT_UINT) continue;
        
        // result = e^(-k*(tube-hand)^2)
        uint32_t result = fixed_point_gaussian(x_tube);
        // result = Ae^(-k*(tube-hand)^2)
        result = (result * A) >> 16;
        // result = min(Ae^(-k*(tube-hand)^2), 1.0)    (may have A>1)
        if (result > FP_1_0) result = FP_1_0;
        // update the maximum
        if (result > pwm_raw[tube]) pwm_raw[tube] = result;
    }
}

// hand_flags byte = .....HMS
void update_pwm_raw_from_hands(uint8_t hand_flags)
{
    memset(pwm_raw, 0, sizeof(uint32_t) * N_TUBES);
    for (uint h = 0; h < N_HANDS; h++)
    {
        if (hand_flags & 1) {
            calculate_single_hand(hand_pos[h], hand_k[h], hand_A[h]);
        }
        hand_flags = hand_flags >> 1;
    }
}

// tsync.update_pwm_raw_from_hands(hand_flags: int = 7)
static mp_obj_t tsync_update_pwm_raw_from_hands(mp_uint_t n_args, mp_obj_t* args) {
    uint8_t hand_flags = DEFAULT_HAND_FLAGS; // default is to draw all 3 hands
    if (n_args > 0) {
        if (!(mp_obj_is_int(args[0]))) {
            mp_raise_TypeError("hand_flags must be int");
        }
        int hand_flags_in = mp_obj_get_int(args[0]);
        if (hand_flags_in < 0 || hand_flags_in >= DEFAULT_HAND_FLAGS + 1) {
            mp_raise_ValueError("hand_flags must be in range 0..7");
        }
        hand_flags = (uint8_t)hand_flags_in;
    }
    update_pwm_raw_from_hands(hand_flags);
    return mp_const_none;
}
MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(tsync_update_pwm_raw_from_hands_obj, 0, 1, tsync_update_pwm_raw_from_hands);

void update_pwm_raw_from_single_gaussian(uint16_t pos, uint32_t k, uint32_t A) {
    memset(pwm_raw, 0, sizeof(uint32_t) * N_TUBES);
    calculate_single_hand(pos, k, A);
}
static mp_obj_t tsync_update_pwm_raw_from_single_gaussian(mp_obj_t pos_obj, mp_obj_t k_obj, mp_obj_t A_obj) {
    if (!mp_obj_is_int(pos_obj) || !mp_obj_is_int(k_obj) || !mp_obj_is_int(A_obj)) {
        mp_raise_TypeError("All inputs must be ints");
    }
    uint16_t pos = mp_obj_get_int(pos_obj);
    uint32_t k = mp_obj_get_k(k_obj);
    uint32_t A = mp_obj_get_A(A_obj);
    update_pwm_raw_from_single_gaussian(pos, k, A);
    return mp_const_none;
}
MP_DEFINE_CONST_FUN_OBJ_3(tsync_update_pwm_raw_from_single_gaussian_obj, tsync_update_pwm_raw_from_single_gaussian);