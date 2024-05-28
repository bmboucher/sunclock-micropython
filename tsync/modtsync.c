#include "tsync.h"

// Define all attributes of the module.
// Table entries are key/value pairs of the attribute name (a string)
// and the MicroPython object reference.
// All identifiers and strings are written as MP_QSTR_xxx and will be
// optimized to word-sized integers by the build system (interned strings).
static const mp_rom_map_elem_t tsync_globals_table[] = {
    // module name
    {MP_ROM_QSTR(MP_QSTR___name__), MP_ROM_QSTR(MP_QSTR_tsync)},
    // global variables
    TSYNC_GLOBAL(is_primary),
    TSYNC_GLOBAL(set_is_primary),
    TSYNC_GLOBAL(core1_running),
    TSYNC_GLOBAL(core1_cancel),
    TSYNC_GLOBAL(set_core1_cancel),
    TSYNC_GLOBAL(manual_hand_pos),
    TSYNC_GLOBAL(set_manual_hand_pos),
    TSYNC_GLOBAL(manual_pwm_raw),
    TSYNC_GLOBAL(set_manual_pwm_raw),
    TSYNC_GLOBAL(manual_pwm_duty),
    TSYNC_GLOBAL(set_manual_pwm_duty),
    TSYNC_GLOBAL(rx_errors),
    TSYNC_GLOBAL(frame_rate),
    // buffers
    TSYNC_GLOBAL(get_time),
    TSYNC_GLOBAL(get_time_adj),
    TSYNC_GLOBAL(get_hand_k),
    TSYNC_GLOBAL(get_hand_A),
    TSYNC_GLOBAL(get_hand_pos),
    TSYNC_GLOBAL(get_pwm_raw),
    TSYNC_GLOBAL(get_pwm_lo),
    TSYNC_GLOBAL(get_pwm_hi),
    TSYNC_GLOBAL(get_pwm_duty),
    TSYNC_GLOBAL(get_tube_map),
    TSYNC_GLOBAL(get_pin_map),
    TSYNC_GLOBAL(get_i2c_buffer),
    // clock
    TSYNC_GLOBAL(update_time),
    TSYNC_GLOBAL(update_hand_pos),
    TSYNC_GLOBAL(epoch_millis),
    TSYNC_GLOBAL(epoch_seconds_float),
    TSYNC_GLOBAL(localtime),
    TSYNC_GLOBAL(get_dst_info),
    // hands
    TSYNC_GLOBAL(fixed_point_exp),
    TSYNC_GLOBAL(fixed_point_gaussian),
    TSYNC_GLOBAL(update_pwm_raw_from_hands),
    TSYNC_GLOBAL(update_pwm_raw_from_single_gaussian),
    // i2c
    TSYNC_GLOBAL(update_i2c_tx_buffer),
    TSYNC_GLOBAL(update_i2c_rx_buffer),
    TSYNC_GLOBAL(i2c_tx_send),
    // pwm
    TSYNC_GLOBAL(get_pwm_bits),
    TSYNC_GLOBAL(set_pwm_bits),
    TSYNC_GLOBAL(update_pwm_duty),
    TSYNC_GLOBAL(paint_pwm),
    // core1
    TSYNC_GLOBAL(core1_init),
    TSYNC_GLOBAL(core1_reset_soon),
    TSYNC_GLOBAL(draw_loop_primary),
    TSYNC_GLOBAL(draw_loop_secondary)
};
static MP_DEFINE_CONST_DICT(tsync_globals, tsync_globals_table);

// Define module object.
const mp_obj_module_t tsync = {
    .base = {&mp_type_module},
    .globals = (mp_obj_dict_t *)&tsync_globals,
};

// Register the module to make it available in Python.
MP_REGISTER_MODULE(MP_QSTR_tsync, tsync);