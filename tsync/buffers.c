#include "tsync.h"

// define extern buffers
uint16_t time[7]; // y,m,d,h,m,s,ms
uint32_t hand_k[N_HANDS];
uint32_t hand_A[N_HANDS];
uint16_t hand_pos[N_HANDS];
uint32_t pwm_raw[N_TUBES];
uint16_t pwm_lo[N_TUBES];
uint16_t pwm_hi[N_TUBES];
uint16_t pwm_duty[N_TUBES];
uint8_t tube_map[N_TUBES];
uint8_t pin_map[N_TUBES];
uint16_t uart_buffer[N_TUBES_HALF + 2]; // start data[0] ... data[11] checksum

// tsync.get_buffers() -> Tuple[memoryview, ... memoryview]
static mp_obj_t tsync_get_buffers(void) {
    // Create a Python tuple to hold the memoryview objects
    mp_obj_t tuple[11];
    
    // Initialize memoryview objects for each array and store them in the tuple
    tuple[0] = mp_obj_new_memoryview('H', 7, time);
    tuple[1] = mp_obj_new_memoryview('I', N_HANDS, hand_k);
    tuple[2] = mp_obj_new_memoryview('I', N_HANDS, hand_A);
    tuple[3] = mp_obj_new_memoryview('H', N_HANDS, hand_pos);
    tuple[4] = mp_obj_new_memoryview('I', N_TUBES, pwm_raw);
    tuple[5] = mp_obj_new_memoryview('H', N_TUBES, pwm_lo);
    tuple[6] = mp_obj_new_memoryview('H', N_TUBES, pwm_hi);
    tuple[7] = mp_obj_new_memoryview('H', N_TUBES, pwm_duty);
    tuple[8] = mp_obj_new_memoryview('B', N_TUBES, tube_map);
    tuple[9] = mp_obj_new_memoryview('B', N_TUBES, pin_map);
    tuple[10] = mp_obj_new_memoryview('H', N_TUBES_HALF + 2, uart_buffer);
    
    // Return the tuple
    return mp_obj_new_tuple(10, tuple);
}

MP_DEFINE_CONST_FUN_OBJ_0(tsync_get_buffers_obj, tsync_get_buffers);