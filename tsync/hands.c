#include "tsync.h"
#include <string.h>

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

static uint32_t safe_multiply(uint32_t a, uint32_t b) {
    uint scale = 0;
    while (a >= FP_1_0) {
        scale += 1;
        a = a>>1;
    }
    while (b >= FP_1_0) {
        scale += 1;
        b = b>>1;
    }
    return (a*b)>>(16-scale);
}

// calculate e^(-input^2)
uint32_t fixed_point_gaussian(uint32_t input)
{
    if (input >= GAUSS_LIMIT_UINT) return 0;
    const uint32_t input_sq = safe_multiply(input, input);
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

const uint32_t TUBE_STEP = FP_1_0/N_TUBES;

static void update_single_hand(uint32_t pos, uint32_t k, uint32_t A)
{
    uint32_t tube_frac = 0;
    for (uint32_t tube = 0; tube < N_TUBES; tube++) {
        // calculate x_tube = k*abs(tube - hand)
        uint32_t x_tube = tube_frac > pos ? tube_frac - pos : pos - tube_frac;
        if (x_tube >= FP_0_5) x_tube = FP_1_0 - x_tube;
        x_tube = safe_multiply(x_tube, k);
        tube_frac += TUBE_STEP;  // update for the next iteration

        // will return 0 from fixed_point_exp anyway
        if (x_tube >= GAUSS_LIMIT_UINT) continue;
        
        // result = e^(-k*(tube-hand)^2)
        uint32_t result = fixed_point_gaussian(x_tube);
        // result = Ae^(-k*(tube-hand)^2)
        result = safe_multiply(result, A);
        // result = min(Ae^(-k*(tube-hand)^2), 1.0)    (may have A>1)
        if (result > FP_1_0) result = FP_1_0;
        // update the maximum
        if (result > pwm_raw[tube]) pwm_raw[tube] = result;
    }
}

// hand_flags byte = .....HMS
void update_pwm_raw_from_hands(uint8_t hand_flags)
{
    for (uint t = 0; t < N_TUBES; t++) {
        pwm_raw[t] = 0;
    }
    for (uint h = 0; h < N_HANDS; h++)
    {
        if (hand_flags & 1) {
            update_single_hand((uint32_t)hand_pos[h], hand_k[h], hand_A[h]);
        }
        hand_flags = hand_flags >> 1;
    }
}

// tsync.update_pwm_raw_from_hands(hand_flags: int = 7)
static mp_obj_t tsync_update_pwm_raw_from_hands(mp_uint_t n_args, const mp_obj_t* args) {
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
    for (uint t = 0; t < N_TUBES; t++) {
        pwm_raw[t] = 0;
    }
    update_single_hand(pos, k, A);
}
static mp_obj_t tsync_update_pwm_raw_from_single_gaussian(mp_obj_t pos_obj, mp_obj_t k_obj, mp_obj_t A_obj) {
    if (!mp_obj_is_int(pos_obj) || !mp_obj_is_int(k_obj) || !mp_obj_is_int(A_obj)) {
        mp_raise_TypeError("All inputs must be ints");
    }
    uint16_t pos = mp_obj_get_int(pos_obj);
    uint32_t k = mp_obj_get_int(k_obj);
    uint32_t A = mp_obj_get_int(A_obj);
    update_pwm_raw_from_single_gaussian(pos, k, A);
    return mp_const_none;
}
MP_DEFINE_CONST_FUN_OBJ_3(tsync_update_pwm_raw_from_single_gaussian_obj, tsync_update_pwm_raw_from_single_gaussian);