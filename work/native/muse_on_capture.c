#include "muse_on_capture.h"

#include <IOKit/hid/IOHIDManager.h>

uint32_t muse_on_joystick_open_options(bool capture_requested) {
  return capture_requested ? (uint32_t)kIOHIDOptionsTypeSeizeDevice
                           : (uint32_t)kIOHIDOptionsTypeNone;
}

uint32_t muse_on_keyboard_capture_options(void) {
  return (uint32_t)kIOHIDOptionsTypeNone;
}

bool muse_on_capture_is_verified(bool joystick_open,
                                 bool keyboard_filter_verified,
                                 bool controller_connected) {
  return joystick_open && keyboard_filter_verified && controller_connected;
}
