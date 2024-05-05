#include "tsync.h"

void *get_array(mp_obj_t array, char typecode, uint size)
{
    // check input is an array of the proper size and get the C pointer
    mp_buffer_info_t buffer_info;
    mp_get_buffer_raise(array, &buffer_info, MP_BUFFER_RW);
    if (!mp_obj_is_type(array, &mp_type_array))
    {
        mp_raise_TypeError("Array argument expected");
    }
    else if (buffer_info.typecode != typecode)
    {
        mp_raise_TypeError("Array has wrong typecode");
    }
    else if (buffer_info.len < size)
    {
        mp_raise_TypeError("Array is too small");
    }
    return buffer_info.buf;
}