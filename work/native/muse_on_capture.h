#ifndef MUSE_ON_CAPTURE_H
#define MUSE_ON_CAPTURE_H

#include <stdbool.h>
#include <stdint.h>

uint32_t muse_on_capture_open_options(void);
bool muse_on_capture_is_verified(bool keyboard_open, bool joystick_open,
                                 bool single_controller);
bool muse_on_capture_close_result_is_acceptable(int32_t result);

#endif
