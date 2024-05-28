# Create an INTERFACE library for our C module.
add_library(tsync INTERFACE)

# generate PIO headers
pico_generate_pio_header(tsync ${CMAKE_CURRENT_LIST_DIR}/i2c.pio)

# Add our source files to the lib
target_sources(tsync INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}/buffers.c
    ${CMAKE_CURRENT_LIST_DIR}/clock.c
    ${CMAKE_CURRENT_LIST_DIR}/core1.c
    ${CMAKE_CURRENT_LIST_DIR}/hands.c
    ${CMAKE_CURRENT_LIST_DIR}/modtsync.c
    ${CMAKE_CURRENT_LIST_DIR}/pwm.c
    ${CMAKE_CURRENT_LIST_DIR}/i2c.c
)

# Add the current directory as an include directory.
target_include_directories(tsync INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}
)

# Link our INTERFACE library to the usermod target.
target_link_libraries(usermod INTERFACE tsync)
