#include "muse_on_capture.h"

#include <IOKit/IOReturn.h>
#include <IOKit/hid/IOHIDManager.h>

uint32_t muse_on_capture_open_options(void) {
  return (uint32_t)kIOHIDOptionsTypeSeizeDevice;
}

bool muse_on_capture_is_verified(bool keyboard_open, bool joystick_open,
                                 bool single_controller) {
  return keyboard_open && joystick_open && single_controller;
}

bool muse_on_capture_close_result_is_acceptable(int32_t result) {
  return result == kIOReturnSuccess || result == kIOReturnNotOpen ||
         result == kIOReturnNoDevice || result == kIOReturnOffline;
}
