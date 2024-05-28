#include "tsync.h"
#include "py/objarray.h"

// define extern buffers
uint16_t time[7]; // y,m,d,h,m,s,ms
int32_t time_adj[4]; // h,m,s,ms
uint32_t hand_k[N_HANDS];
uint32_t hand_A[N_HANDS];
uint16_t hand_pos[N_HANDS];
uint32_t pwm_raw[N_TUBES];
uint16_t pwm_lo[N_TUBES];
uint16_t pwm_hi[N_TUBES];
uint16_t pwm_duty[N_TUBES];
uint8_t tube_map[N_TUBES];
uint8_t pin_map[N_TUBES] = {0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,26};
uint16_t i2c_buffer[N_TUBES_HALF + 2]; // start data[0] ... data[11] checksum

// tsync.get_time() -> memoryview
static mp_obj_t tsync_get_time(void) {
    return mp_obj_new_memoryview(MP_OBJ_ARRAY_TYPECODE_FLAG_RW | 'H', 7, time);
}

// tsync.get_time_adj() -> memoryview
static mp_obj_t tsync_get_time_adj(void) {
    return mp_obj_new_memoryview(MP_OBJ_ARRAY_TYPECODE_FLAG_RW | 'i', 4, time_adj);
}

// tsync.get_hand_k() -> memoryview
static mp_obj_t tsync_get_hand_k(void) {
    return mp_obj_new_memoryview(MP_OBJ_ARRAY_TYPECODE_FLAG_RW | 'I', N_HANDS, hand_k);
}

// tsync.get_hand_A() -> memoryview
static mp_obj_t tsync_get_hand_A(void) {
    return mp_obj_new_memoryview(MP_OBJ_ARRAY_TYPECODE_FLAG_RW | 'I', N_HANDS, hand_A);
}

// tsync.get_hand_pos() -> memoryview
static mp_obj_t tsync_get_hand_pos(void) {
    return mp_obj_new_memoryview(MP_OBJ_ARRAY_TYPECODE_FLAG_RW | 'H', N_HANDS, hand_pos);
}

// tsync.get_pwm_raw() -> memoryview
static mp_obj_t tsync_get_pwm_raw(void) {
    return mp_obj_new_memoryview(MP_OBJ_ARRAY_TYPECODE_FLAG_RW | 'I', N_TUBES, pwm_raw);
}

// tsync.get_pwm_lo() -> memoryview
static mp_obj_t tsync_get_pwm_lo(void) {
    return mp_obj_new_memoryview(MP_OBJ_ARRAY_TYPECODE_FLAG_RW | 'H', N_TUBES, pwm_lo);
}

// tsync.get_pwm_hi() -> memoryview
static mp_obj_t tsync_get_pwm_hi(void) {
    return mp_obj_new_memoryview(MP_OBJ_ARRAY_TYPECODE_FLAG_RW | 'H', N_TUBES, pwm_hi);
}

// tsync.get_pwm_duty() -> memoryview
static mp_obj_t tsync_get_pwm_duty(void) {
    return mp_obj_new_memoryview(MP_OBJ_ARRAY_TYPECODE_FLAG_RW | 'H', N_TUBES, pwm_duty);
}

// tsync.get_tube_map() -> memoryview
static mp_obj_t tsync_get_tube_map(void) {
    return mp_obj_new_memoryview(MP_OBJ_ARRAY_TYPECODE_FLAG_RW | 'B', N_TUBES, tube_map);
}

// tsync.get_pin_map() -> memoryview
static mp_obj_t tsync_get_pin_map(void) {
    return mp_obj_new_memoryview(MP_OBJ_ARRAY_TYPECODE_FLAG_RW | 'B', N_TUBES, pin_map);
}

// tsync.get_i2c_buffer() -> memoryview
static mp_obj_t tsync_get_i2c_buffer(void) {
    return mp_obj_new_memoryview(MP_OBJ_ARRAY_TYPECODE_FLAG_RW | 'H', N_TUBES_HALF + 2, i2c_buffer);
}

// Define method table for each get_<name> method
MP_DEFINE_CONST_FUN_OBJ_0(tsync_get_time_obj, tsync_get_time);
MP_DEFINE_CONST_FUN_OBJ_0(tsync_get_time_adj_obj, tsync_get_time_adj);
MP_DEFINE_CONST_FUN_OBJ_0(tsync_get_hand_k_obj, tsync_get_hand_k);
MP_DEFINE_CONST_FUN_OBJ_0(tsync_get_hand_A_obj, tsync_get_hand_A);
MP_DEFINE_CONST_FUN_OBJ_0(tsync_get_hand_pos_obj, tsync_get_hand_pos);
MP_DEFINE_CONST_FUN_OBJ_0(tsync_get_pwm_raw_obj, tsync_get_pwm_raw);
MP_DEFINE_CONST_FUN_OBJ_0(tsync_get_pwm_lo_obj, tsync_get_pwm_lo);
MP_DEFINE_CONST_FUN_OBJ_0(tsync_get_pwm_hi_obj, tsync_get_pwm_hi);
MP_DEFINE_CONST_FUN_OBJ_0(tsync_get_pwm_duty_obj, tsync_get_pwm_duty);
MP_DEFINE_CONST_FUN_OBJ_0(tsync_get_tube_map_obj, tsync_get_tube_map);
MP_DEFINE_CONST_FUN_OBJ_0(tsync_get_pin_map_obj, tsync_get_pin_map);
MP_DEFINE_CONST_FUN_OBJ_0(tsync_get_i2c_buffer_obj, tsync_get_i2c_buffer);