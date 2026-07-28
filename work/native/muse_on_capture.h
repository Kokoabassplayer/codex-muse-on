#ifndef MUSE_ON_CAPTURE_H
#define MUSE_ON_CAPTURE_H

#include <stdbool.h>
#include <stdint.h>

uint32_t muse_on_joystick_open_options(bool capture_requested);
uint32_t muse_on_keyboard_capture_options(void);
bool muse_on_capture_is_verified(bool joystick_open,
                                 bool keyboard_filter_verified,
                                 bool controller_connected);

#endif
