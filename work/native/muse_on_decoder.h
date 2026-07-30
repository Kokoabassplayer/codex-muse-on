#ifndef MUSE_ON_DECODER_H
#define MUSE_ON_DECODER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Keyboard reports can contain 8 modifiers plus 30 releases and 30 presses.
 * Events are
 * ordered by interface field order: joystick axes, hat, buttons; keyboard
 * modifiers, array releases, array presses; mouse buttons, X, Y, wheel. */
#define MUSE_ON_MAX_EVENTS_PER_REPORT 68

typedef enum {
  MUSE_ON_INTERFACE_JOYSTICK = 1,
  MUSE_ON_INTERFACE_KEYBOARD_BOOT,
  MUSE_ON_INTERFACE_MOUSE
} MuseOnInterfaceKind;

typedef enum {
  MUSE_ON_EVENT_NONE = 0,
  MUSE_ON_EVENT_WHITE1_DOWN,
  MUSE_ON_EVENT_WHITE1_UP,
  MUSE_ON_EVENT_BLACK2_DOWN,
  MUSE_ON_EVENT_BLACK2_UP,
  MUSE_ON_EVENT_BLACK6_DOWN,
  MUSE_ON_EVENT_BLACK6_UP,
  MUSE_ON_EVENT_BLACK8_DOWN,
  MUSE_ON_EVENT_BLACK8_UP,
  MUSE_ON_EVENT_PEDAL_DOWN,
  MUSE_ON_EVENT_PEDAL_UP,
  MUSE_ON_EVENT_WHITE3_DOWN,
  MUSE_ON_EVENT_WHITE3_UP,
  MUSE_ON_EVENT_BLACK4_DOWN,
  MUSE_ON_EVENT_BLACK4_UP,
  MUSE_ON_EVENT_WHITE5_DOWN,
  MUSE_ON_EVENT_WHITE5_UP,
  MUSE_ON_EVENT_WHITE7_DOWN,
  MUSE_ON_EVENT_WHITE7_UP,
  MUSE_ON_EVENT_LEFT_BALL_NORTH_ENGAGED,
  MUSE_ON_EVENT_LEFT_BALL_NORTH_RELEASED,
  MUSE_ON_EVENT_LEFT_BALL_SOUTH_ENGAGED,
  MUSE_ON_EVENT_LEFT_BALL_SOUTH_RELEASED,
  MUSE_ON_EVENT_RIGHT_BALL_VERTICAL_ENGAGED,
  MUSE_ON_EVENT_RIGHT_BALL_VERTICAL_RELEASED,
  MUSE_ON_EVENT_RIGHT_BALL_WEST_ENGAGED,
  MUSE_ON_EVENT_RIGHT_BALL_WEST_RELEASED,
  MUSE_ON_EVENT_RIGHT_BALL_EAST_ENGAGED,
  MUSE_ON_EVENT_RIGHT_BALL_EAST_RELEASED,
  MUSE_ON_EVENT_TURNTABLE_CLOCKWISE_ENGAGED,
  MUSE_ON_EVENT_TURNTABLE_CLOCKWISE_RELEASED,
  MUSE_ON_EVENT_TURNTABLE_COUNTERCLOCKWISE_ENGAGED,
  MUSE_ON_EVENT_TURNTABLE_COUNTERCLOCKWISE_RELEASED,
  MUSE_ON_EVENT_KEY_USAGE_DOWN,
  MUSE_ON_EVENT_KEY_USAGE_UP,
  MUSE_ON_EVENT_MOUSE_BUTTON_DOWN,
  MUSE_ON_EVENT_MOUSE_BUTTON_UP,
  MUSE_ON_EVENT_MOUSE_RELATIVE_X,
  MUSE_ON_EVENT_MOUSE_RELATIVE_Y,
  MUSE_ON_EVENT_MOUSE_WHEEL,
  MUSE_ON_EVENT_JOYSTICK_AXIS,
  MUSE_ON_EVENT_JOYSTICK_HAT,
  MUSE_ON_EVENT_JOYSTICK_BUTTON_DOWN,
  MUSE_ON_EVENT_JOYSTICK_BUTTON_UP
} MuseOnEventName;

typedef struct {
  MuseOnInterfaceKind interface_kind;
  uint8_t report_id;
  MuseOnEventName name;
  uint8_t byte_index;
  uint8_t bit_mask;
  uint8_t usage;
  int32_t value;
} MuseOnEvent;

typedef struct {
  bool joystick_report_seen;
  uint8_t joystick_axes[7];
  uint16_t joystick_hat;
  uint16_t joystick_buttons;
  bool keyboard_usages_seen;
  uint8_t keyboard_modifiers;
  uint8_t keyboard_usages[30];
  bool mouse_buttons_seen;
  uint8_t mouse_button_byte;
} MuseOnDecoder;

void muse_on_decoder_init(MuseOnDecoder *decoder);
size_t muse_on_decode_report(MuseOnDecoder *decoder,
                             MuseOnInterfaceKind interface_kind,
                             uint8_t report_id,
                             const uint8_t *bytes,
                             size_t byte_count,
                             MuseOnEvent *events,
                             size_t event_capacity);

#endif
