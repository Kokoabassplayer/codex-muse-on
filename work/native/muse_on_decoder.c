#include "muse_on_decoder.h"

void muse_on_decoder_init(MuseOnDecoder *decoder) {
  size_t index;

  for (index = 0; index < 7; index++) {
    decoder->joystick_axes[index] = 0x80;
  }
  decoder->joystick_hat = 0xffff;
  decoder->joystick_buttons = 0;
  decoder->keyboard_usages_seen = false;
  decoder->keyboard_modifiers = 0;
  for (index = 0; index < 30; index++) {
    decoder->keyboard_usages[index] = 0;
  }
  decoder->mouse_buttons_seen = false;
  decoder->mouse_button_byte = 0;
}

static void set_joystick_button_event(MuseOnEvent *event, uint8_t report_id,
                                      uint8_t bit_mask, MuseOnEventName down,
                                      MuseOnEventName up, uint8_t buttons) {
  event->interface_kind = MUSE_ON_INTERFACE_JOYSTICK;
  event->report_id = report_id;
  event->name = (buttons & bit_mask) ? down : up;
  event->byte_index = 9;
  event->bit_mask = bit_mask;
  event->usage = 0;
  event->value = 0;
}

static bool contains_usage(const uint8_t *usages, uint8_t usage) {
  size_t index;

  for (index = 0; index < 30; index++) {
    if (usages[index] == usage) return true;
  }
  return false;
}

static MuseOnEventName keyboard_usage_event_name(uint8_t usage, bool down) {
  switch (usage) {
    case 0x27:
      return down ? MUSE_ON_EVENT_WHITE3_DOWN : MUSE_ON_EVENT_WHITE3_UP;
    case 0x26:
      return down ? MUSE_ON_EVENT_BLACK4_DOWN : MUSE_ON_EVENT_BLACK4_UP;
    case 0x50:
      return down ? MUSE_ON_EVENT_WHITE5_DOWN : MUSE_ON_EVENT_WHITE5_UP;
    case 0x4f:
      return down ? MUSE_ON_EVENT_WHITE7_DOWN : MUSE_ON_EVENT_WHITE7_UP;
    case 0x3d:
      return down ? MUSE_ON_EVENT_LEFT_BALL_NORTH_ENGAGED
                  : MUSE_ON_EVENT_LEFT_BALL_NORTH_RELEASED;
    case 0x3c:
      return down ? MUSE_ON_EVENT_LEFT_BALL_SOUTH_ENGAGED
                  : MUSE_ON_EVENT_LEFT_BALL_SOUTH_RELEASED;
    case 0x1e:
      return down ? MUSE_ON_EVENT_RIGHT_BALL_VERTICAL_ENGAGED
                  : MUSE_ON_EVENT_RIGHT_BALL_VERTICAL_RELEASED;
    case 0x1f:
      return down ? MUSE_ON_EVENT_RIGHT_BALL_WEST_ENGAGED
                  : MUSE_ON_EVENT_RIGHT_BALL_WEST_RELEASED;
    case 0x20:
      return down ? MUSE_ON_EVENT_RIGHT_BALL_EAST_ENGAGED
                  : MUSE_ON_EVENT_RIGHT_BALL_EAST_RELEASED;
    default:
      return down ? MUSE_ON_EVENT_KEY_USAGE_DOWN : MUSE_ON_EVENT_KEY_USAGE_UP;
  }
}

static MuseOnEventName joystick_hat_event_name(uint16_t hat, bool engaged) {
  switch (hat) {
    case 0x005a:
      return engaged ? MUSE_ON_EVENT_TURNTABLE_CLOCKWISE_ENGAGED
                     : MUSE_ON_EVENT_TURNTABLE_CLOCKWISE_RELEASED;
    case 0x010e:
      return engaged ? MUSE_ON_EVENT_TURNTABLE_COUNTERCLOCKWISE_ENGAGED
                     : MUSE_ON_EVENT_TURNTABLE_COUNTERCLOCKWISE_RELEASED;
    default:
      return MUSE_ON_EVENT_NONE;
  }
}

static void set_joystick_hat_event(MuseOnEvent *event, uint8_t report_id,
                                   MuseOnEventName name, uint16_t value) {
  event->interface_kind = MUSE_ON_INTERFACE_JOYSTICK;
  event->report_id = report_id;
  event->name = name;
  event->byte_index = 7;
  event->bit_mask = 0;
  event->usage = 0x39;
  event->value = value;
}

size_t muse_on_decode_report(MuseOnDecoder *decoder,
                             MuseOnInterfaceKind interface_kind,
                             uint8_t report_id,
                             const uint8_t *bytes,
                             size_t byte_count,
                             MuseOnEvent *events,
                             size_t event_capacity) {
  uint8_t current_buttons;
  size_t index;
  size_t event_count = 0;

  if (!decoder || !bytes || (!events && event_capacity)) {
    return 0;
  }

  if (interface_kind == MUSE_ON_INTERFACE_KEYBOARD_BOOT) {
    uint8_t changed_modifiers;
    uint8_t bit_mask;
    uint8_t modifier_usage;

    if (byte_count != 32) return 0;
    if (!decoder->keyboard_usages_seen) {
      decoder->keyboard_usages_seen = true;
    }
    changed_modifiers = (uint8_t)(decoder->keyboard_modifiers ^ bytes[0]);
    decoder->keyboard_modifiers = bytes[0];
    for (bit_mask = 0x01, modifier_usage = 0xe0; bit_mask && event_count < event_capacity;
         bit_mask = (uint8_t)(bit_mask << 1), modifier_usage++) {
      if (changed_modifiers & bit_mask) {
        events[event_count].interface_kind = interface_kind;
        events[event_count].report_id = report_id;
        events[event_count].name = (bytes[0] & bit_mask)
          ? MUSE_ON_EVENT_KEY_USAGE_DOWN : MUSE_ON_EVENT_KEY_USAGE_UP;
        events[event_count].byte_index = 0;
        events[event_count].bit_mask = bit_mask;
        events[event_count].usage = modifier_usage;
        events[event_count++].value = 0;
      }
    }
    for (index = 0; index < 30 && event_count < event_capacity; index++) {
      uint8_t usage = decoder->keyboard_usages[index];
      if (usage && !contains_usage(bytes + 2, usage)) {
        events[event_count].interface_kind = interface_kind;
        events[event_count].report_id = report_id;
        events[event_count].name = keyboard_usage_event_name(usage, false);
        events[event_count].byte_index = (uint8_t)(index + 2);
        events[event_count].bit_mask = 0;
        events[event_count].usage = usage;
        events[event_count++].value = 0;
      }
    }
    for (index = 0; index < 30 && event_count < event_capacity; index++) {
      uint8_t usage = bytes[index + 2];
      if (usage && !contains_usage(decoder->keyboard_usages, usage)) {
        events[event_count].interface_kind = interface_kind;
        events[event_count].report_id = report_id;
        events[event_count].name = keyboard_usage_event_name(usage, true);
        events[event_count].byte_index = (uint8_t)(index + 2);
        events[event_count].bit_mask = 0;
        events[event_count].usage = usage;
        events[event_count++].value = 0;
      }
    }
    for (index = 0; index < 30; index++) decoder->keyboard_usages[index] = bytes[index + 2];
    return event_count;
  }

  if (interface_kind == MUSE_ON_INTERFACE_MOUSE) {
    uint8_t changed_buttons;
    uint8_t bit_mask;

    if (byte_count != 4) return 0;
    if (!decoder->mouse_buttons_seen) {
      decoder->mouse_buttons_seen = true;
    }
    changed_buttons = (uint8_t)(decoder->mouse_button_byte ^ bytes[0]);
    decoder->mouse_button_byte = bytes[0];
    for (bit_mask = 0x01; bit_mask <= 0x04 && event_count < event_capacity;
         bit_mask = (uint8_t)(bit_mask << 1)) {
      if (changed_buttons & bit_mask) {
        events[event_count].interface_kind = interface_kind;
        events[event_count].report_id = report_id;
        events[event_count].name = (bytes[0] & bit_mask)
          ? MUSE_ON_EVENT_MOUSE_BUTTON_DOWN : MUSE_ON_EVENT_MOUSE_BUTTON_UP;
        events[event_count].byte_index = 0;
        events[event_count].bit_mask = bit_mask;
        events[event_count].usage = 0;
        events[event_count++].value = 0;
      }
    }
    if (bytes[1] && event_count < event_capacity) {
      events[event_count].interface_kind = interface_kind;
      events[event_count].report_id = report_id;
      events[event_count].name = MUSE_ON_EVENT_MOUSE_RELATIVE_X;
      events[event_count].byte_index = 1;
      events[event_count].bit_mask = 0;
      events[event_count].usage = 0;
      events[event_count++].value = (int8_t)bytes[1];
    }
    if (bytes[2] && event_count < event_capacity) {
      events[event_count].interface_kind = interface_kind;
      events[event_count].report_id = report_id;
      events[event_count].name = MUSE_ON_EVENT_MOUSE_RELATIVE_Y;
      events[event_count].byte_index = 2;
      events[event_count].bit_mask = 0;
      events[event_count].usage = 0;
      events[event_count++].value = (int8_t)bytes[2];
    }
    if (bytes[3] && event_count < event_capacity) {
      events[event_count].interface_kind = interface_kind;
      events[event_count].report_id = report_id;
      events[event_count].name = MUSE_ON_EVENT_MOUSE_WHEEL;
      events[event_count].byte_index = 3;
      events[event_count].bit_mask = 0;
      events[event_count].usage = 0;
      events[event_count++].value = (int8_t)bytes[3];
    }
    return event_count;
  }

  if (interface_kind != MUSE_ON_INTERFACE_JOYSTICK || report_id != 1) return 0;
  if (byte_count == 12) {
    if (bytes[0] != report_id) return 0;
    bytes++;
    byte_count--;
  }
  if (byte_count != 11) return 0;

  for (index = 0; index < 7 && event_count < event_capacity; index++) {
    if (bytes[index] != decoder->joystick_axes[index]) {
      events[event_count].interface_kind = interface_kind;
      events[event_count].report_id = report_id;
      events[event_count].name = MUSE_ON_EVENT_JOYSTICK_AXIS;
      events[event_count].byte_index = (uint8_t)index;
      events[event_count].bit_mask = 0;
      events[event_count].usage = (uint8_t)(0x30 + index);
      events[event_count++].value = bytes[index];
    }
  }
  for (index = 0; index < 7; index++) decoder->joystick_axes[index] = bytes[index];

  {
    uint16_t current_hat = (uint16_t)bytes[7] | ((uint16_t)bytes[8] << 8);
    if (current_hat != decoder->joystick_hat) {
      MuseOnEventName released = joystick_hat_event_name(decoder->joystick_hat, false);
      MuseOnEventName engaged = joystick_hat_event_name(current_hat, true);

      if (released != MUSE_ON_EVENT_NONE && event_count < event_capacity) {
        set_joystick_hat_event(&events[event_count++], report_id, released,
                               decoder->joystick_hat);
      }
      if (engaged != MUSE_ON_EVENT_NONE && event_count < event_capacity) {
        set_joystick_hat_event(&events[event_count++], report_id, engaged, current_hat);
      }
      if (released == MUSE_ON_EVENT_NONE && engaged == MUSE_ON_EVENT_NONE &&
          event_count < event_capacity) {
        set_joystick_hat_event(&events[event_count++], report_id,
                               MUSE_ON_EVENT_JOYSTICK_HAT, current_hat);
      }
    }
    decoder->joystick_hat = current_hat;
  }

  current_buttons = bytes[9];
  {
    uint16_t current_button_bits = (uint16_t)current_buttons | ((uint16_t)(bytes[10] & 0x0f) << 8);
    uint16_t changed_buttons = decoder->joystick_buttons ^ current_button_bits;
    uint16_t bit;

    decoder->joystick_buttons = current_button_bits;
    for (bit = 0; bit < 12 && event_count < event_capacity; bit++) {
      uint16_t button_mask = (uint16_t)1 << bit;
      uint8_t byte_index = bit < 8 ? 9 : 10;
      uint8_t bit_mask = (uint8_t)(1u << (bit % 8));
      if (!(changed_buttons & button_mask)) continue;
      if (bit == 0) {
        set_joystick_button_event(&events[event_count++], report_id, bit_mask,
                                  MUSE_ON_EVENT_WHITE1_DOWN, MUSE_ON_EVENT_WHITE1_UP, current_buttons);
      } else if (bit == 1) {
        set_joystick_button_event(&events[event_count++], report_id, bit_mask,
                                  MUSE_ON_EVENT_BLACK2_DOWN, MUSE_ON_EVENT_BLACK2_UP, current_buttons);
      } else if (bit == 2) {
        set_joystick_button_event(&events[event_count++], report_id, bit_mask,
                                  MUSE_ON_EVENT_BLACK6_DOWN, MUSE_ON_EVENT_BLACK6_UP, current_buttons);
      } else if (bit == 3) {
        set_joystick_button_event(&events[event_count++], report_id, bit_mask,
                                  MUSE_ON_EVENT_BLACK8_DOWN, MUSE_ON_EVENT_BLACK8_UP, current_buttons);
      } else if (bit == 4) {
        set_joystick_button_event(&events[event_count++], report_id, bit_mask,
                                  MUSE_ON_EVENT_PEDAL_DOWN, MUSE_ON_EVENT_PEDAL_UP, current_buttons);
      } else {
        events[event_count].interface_kind = interface_kind;
        events[event_count].report_id = report_id;
        events[event_count].name = (current_button_bits & button_mask)
          ? MUSE_ON_EVENT_JOYSTICK_BUTTON_DOWN : MUSE_ON_EVENT_JOYSTICK_BUTTON_UP;
        events[event_count].byte_index = byte_index;
        events[event_count].bit_mask = bit_mask;
        events[event_count].usage = (uint8_t)(bit + 1);
        events[event_count++].value = 0;
      }
    }
  }
  return event_count;
}
