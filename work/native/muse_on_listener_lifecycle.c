#include "muse_on_listener_lifecycle.h"

static bool valid_profile(MuseOnProfile profile) {
  return profile == MUSE_ON_PROFILE_CONTROLLER_ONLY ||
         profile == MUSE_ON_PROFILE_PEDAL;
}

static void reset_bound_inputs(MuseOnListenerLifecycle *lifecycle) {
  muse_on_decoder_init(&lifecycle->keyboard_decoder);
  muse_on_decoder_init(&lifecycle->joystick_decoder);
  muse_on_action_router_init(&lifecycle->keyboard_router, lifecycle->profile);
  muse_on_action_router_init(&lifecycle->joystick_router, lifecycle->profile);
  lifecycle->inputs_released = false;
}

static void emit_event(
    MuseOnListenerLifecycle *lifecycle,
    MuseOnListenerLifecycleEvent event) {
  if (lifecycle->sink) {
    lifecycle->sink(lifecycle->sink_context, &event);
  }
}

static void emit_neutral_entry(
    MuseOnListenerLifecycle *lifecycle, bool inputs_released) {
  lifecycle->inputs_released = inputs_released;
  emit_event(
      lifecycle,
      (MuseOnListenerLifecycleEvent){
          .type = MUSE_ON_LISTENER_LIFECYCLE_EVENT_NEUTRAL_ENTRY,
          .inputs_released = inputs_released,
          .controller_location_id = lifecycle->controller_location_id,
      });
}

static MuseOnNeutralEntryState controller_neutral_state(
    const MuseOnListenerLifecycle *lifecycle) {
  MuseOnNeutralEntryState keyboard_state =
      muse_on_selected_profile_neutral_state(
          &lifecycle->keyboard_decoder,
          MUSE_ON_INTERFACE_KEYBOARD_BOOT, lifecycle->profile);
  MuseOnNeutralEntryState joystick_state =
      muse_on_selected_profile_neutral_state(
          &lifecycle->joystick_decoder,
          MUSE_ON_INTERFACE_JOYSTICK, lifecycle->profile);

  if (keyboard_state == MUSE_ON_NEUTRAL_ENTRY_HELD ||
      joystick_state == MUSE_ON_NEUTRAL_ENTRY_HELD) {
    return MUSE_ON_NEUTRAL_ENTRY_HELD;
  }
  if (keyboard_state == MUSE_ON_NEUTRAL_ENTRY_UNKNOWN ||
      joystick_state == MUSE_ON_NEUTRAL_ENTRY_UNKNOWN) {
    return MUSE_ON_NEUTRAL_ENTRY_UNKNOWN;
  }
  return MUSE_ON_NEUTRAL_ENTRY_RELEASED;
}

static void reconcile_neutral_entry(MuseOnListenerLifecycle *lifecycle) {
  bool released =
      controller_neutral_state(lifecycle) == MUSE_ON_NEUTRAL_ENTRY_RELEASED;

  if (released != lifecycle->inputs_released) {
    emit_neutral_entry(lifecycle, released);
  }
}

static void emit_action(
    MuseOnListenerLifecycle *lifecycle, MuseOnInterfaceKind interface_kind,
    MuseOnActionEvent action) {
  emit_event(
      lifecycle,
      (MuseOnListenerLifecycleEvent){
          .type = MUSE_ON_LISTENER_LIFECYCLE_EVENT_ACTION,
          .controller_location_id = lifecycle->controller_location_id,
          .interface_kind = interface_kind,
          .action = action,
      });
}

void muse_on_listener_lifecycle_init(
    MuseOnListenerLifecycle *lifecycle, MuseOnListenerLifecycleSink sink,
    void *sink_context) {
  if (!lifecycle) return;
  *lifecycle = (MuseOnListenerLifecycle){
      .profile = MUSE_ON_PROFILE_CONTROLLER_ONLY,
      .sink = sink,
      .sink_context = sink_context,
  };
  reset_bound_inputs(lifecycle);
}

bool muse_on_listener_lifecycle_bind(
    MuseOnListenerLifecycle *lifecycle, MuseOnProfile profile,
    uint32_t controller_location_id, bool keyboard_present,
    bool joystick_present) {
  if (!lifecycle || !valid_profile(profile) ||
      controller_location_id == 0 || !keyboard_present ||
      !joystick_present) {
    return false;
  }
  if (lifecycle->bound && lifecycle->profile == profile &&
      lifecycle->controller_location_id == controller_location_id &&
      lifecycle->keyboard_present == keyboard_present &&
      lifecycle->joystick_present == joystick_present) {
    return true;
  }
  lifecycle->profile = profile;
  lifecycle->controller_location_id = controller_location_id;
  lifecycle->bound = true;
  lifecycle->keyboard_present = keyboard_present;
  lifecycle->joystick_present = joystick_present;
  reset_bound_inputs(lifecycle);
  return true;
}

bool muse_on_listener_lifecycle_unbind(MuseOnListenerLifecycle *lifecycle) {
  if (!lifecycle || !lifecycle->bound) return false;
  lifecycle->bound = false;
  lifecycle->keyboard_present = false;
  lifecycle->joystick_present = false;
  lifecycle->controller_location_id = 0;
  reset_bound_inputs(lifecycle);
  return true;
}

bool muse_on_listener_lifecycle_focus_changed(
    MuseOnListenerLifecycle *lifecycle, bool foreground) {
  if (!lifecycle ||
      (lifecycle->focus_known && lifecycle->foreground == foreground)) {
    return false;
  }
  lifecycle->focus_known = true;
  lifecycle->foreground = foreground;
  emit_event(
      lifecycle,
      (MuseOnListenerLifecycleEvent){
          .type = MUSE_ON_LISTENER_LIFECYCLE_EVENT_FOCUS_CHANGED,
          .foreground = foreground,
          .controller_location_id = lifecycle->controller_location_id,
      });
  if (!foreground) {
    muse_on_action_router_init(&lifecycle->keyboard_router, lifecycle->profile);
    muse_on_action_router_init(&lifecycle->joystick_router, lifecycle->profile);
    emit_neutral_entry(lifecycle, false);
  }
  return true;
}

bool muse_on_listener_lifecycle_observe_report(
    MuseOnListenerLifecycle *lifecycle, MuseOnInterfaceKind interface_kind,
    uint8_t report_id, const uint8_t *report, size_t report_length,
    uint64_t timestamp_ns, bool external_route_gate) {
  MuseOnInputObservation observation;
  MuseOnDecoder *decoder;
  MuseOnActionRouter *router;
  size_t index;

  if (!lifecycle || !lifecycle->bound) return false;
  if (interface_kind == MUSE_ON_INTERFACE_KEYBOARD_BOOT &&
      lifecycle->keyboard_present) {
    decoder = &lifecycle->keyboard_decoder;
    router = &lifecycle->keyboard_router;
  } else if (interface_kind == MUSE_ON_INTERFACE_JOYSTICK &&
             lifecycle->joystick_present) {
    decoder = &lifecycle->joystick_decoder;
    router = &lifecycle->joystick_router;
  } else {
    return false;
  }
  if (!muse_on_capture_observe_report(
          decoder, interface_kind, lifecycle->profile, report_id, report,
          report_length, &observation)) {
    return false;
  }
  if (!lifecycle->foreground || !lifecycle->inputs_released) {
    reconcile_neutral_entry(lifecycle);
    return true;
  }
  if (!external_route_gate) return true;
  for (index = 0; index < observation.event_count; index++) {
    MuseOnActionEvent action;
    if (muse_on_action_router_route(
            router, observation.events[index].name, timestamp_ns, &action)) {
      emit_action(lifecycle, interface_kind, action);
    }
  }
  return true;
}

void muse_on_listener_lifecycle_tick(
    MuseOnListenerLifecycle *lifecycle, uint64_t timestamp_ns,
    bool external_route_gate) {
  MuseOnActionEvent action;

  if (!lifecycle || !lifecycle->bound || !lifecycle->foreground ||
      !lifecycle->inputs_released || !external_route_gate) {
    return;
  }
  if (muse_on_action_router_tick(
          &lifecycle->keyboard_router, timestamp_ns, &action)) {
    emit_action(lifecycle, MUSE_ON_INTERFACE_KEYBOARD_BOOT, action);
  }
  if (muse_on_action_router_tick(
          &lifecycle->joystick_router, timestamp_ns, &action)) {
    emit_action(lifecycle, MUSE_ON_INTERFACE_JOYSTICK, action);
  }
}

bool muse_on_listener_lifecycle_inputs_released(
    const MuseOnListenerLifecycle *lifecycle) {
  return lifecycle && lifecycle->bound && lifecycle->inputs_released;
}

bool muse_on_listener_lifecycle_is_foreground(
    const MuseOnListenerLifecycle *lifecycle) {
  return lifecycle && lifecycle->focus_known && lifecycle->foreground;
}

bool muse_on_listener_lifecycle_is_bound_to(
    const MuseOnListenerLifecycle *lifecycle, MuseOnProfile profile,
    uint32_t controller_location_id) {
  return lifecycle && lifecycle->bound && lifecycle->profile == profile &&
         lifecycle->controller_location_id == controller_location_id &&
         lifecycle->keyboard_present && lifecycle->joystick_present;
}
