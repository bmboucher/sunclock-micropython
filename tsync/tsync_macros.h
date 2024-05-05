#ifndef __TSYNC_MACROS_H
#define __TSYNC_MACROS_H

#define GLOBAL_RO_INT(name) \
uint name = 0; \
static mp_obj_t tsync_##name(void) { \
    return mp_obj_new_int(name); \
} \
MP_DEFINE_CONST_FUN_OBJ_0(tsync_##name##_obj, tsync_##name)

#define GLOBAL_RO_INT_DECL(name) \
extern uint name; \
MP_DECLARE_CONST_FUN_OBJ_0(tsync_##name##_obj)

#define GLOBAL_FLAG_RO(name) \
bool name = false; \
static mp_obj_t tsync_##name(void) { \
    return mp_obj_new_bool(name); \
} \
MP_DEFINE_CONST_FUN_OBJ_0(tsync_##name##_obj, tsync_##name)

#define GLOBAL_FLAG(name) \
GLOBAL_FLAG_RO(name); \
static mp_obj_t tsync_set_##name(mp_obj_t value) { \
    if (mp_obj_is_bool(value)) name = mp_obj_is_true(value); \
    return mp_const_none; \
} \
MP_DEFINE_CONST_FUN_OBJ_1(tsync_set_##name##_obj, tsync_set_##name)

#define GLOBAL_FLAG_RO_DECL(name) \
extern bool name; \
MP_DECLARE_CONST_FUN_OBJ_0(tsync_##name##_obj)

#define GLOBAL_FLAG_DECL(name) \
GLOBAL_FLAG_RO_DECL(name); \
MP_DECLARE_CONST_FUN_OBJ_0(tsync_set_##name##_obj)

#define TSYNC_GLOBAL(name) \
{MP_ROM_QSTR(MP_QSTR_##name), MP_ROM_PTR(&tsync_##name##_obj)}

// These helper macros define/declare a no-arg/no-return function in C and its
// equivalent function in the tsync Python module
// We use this for "update steps" that we want to call from either side, with
// inputs/outputs being passed through the buffer arrays defined below

#define NO_ARG_MP_FUNCTION(func, contents) \
void func(void) contents \
static mp_obj_t tsync_##func() { \
    func(); \
    return mp_const_none; \
} \
MP_DEFINE_CONST_FUN_OBJ_0(tsync_##func##_obj, tsync_##func);

#define NO_ARG_MP_FUNCTION_DECL(func) \
void func(void); \
MP_DECLARE_CONST_FUN_OBJ_0(tsync_##func##_obj)

#endif //__TSYNC_MACROS_H