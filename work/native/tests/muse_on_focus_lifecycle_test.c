#include "../muse_on_listener_lifecycle.h"
#include "../muse_on_topology.h"

#include <assert.h>

enum { kGeneration = 1 };
static const uint32_t kControllerLocation = UINT32_C(0x110000);

typedef struct {
  MuseOnTopologyHostState host;
  MuseOnState coordinator;
  bool foreground;
  unsigned int focus_count;
  unsigned int neutral_count;
  unsigned int begin_count;
  unsigned int end_count;
} TraceSink;

static MuseOnTopologyCoordinatorInputs coordinator_inputs(
    const TraceSink *sink) {
  return (MuseOnTopologyCoordinatorInputs){
      .permission_granted = true,
      .session_available = true,
      .codex_foreground = sink->foreground,
      .cleanup_verified = true,
  };
}

static void apply_coordinator(TraceSink *sink, MuseOnCommand command) {
  (void)muse_on_topology_host_apply_coordinator(
      &sink->host, &sink->coordinator, command,
      MUSE_ON_SAFETY_FAILURE_NONE, coordinator_inputs(sink));
}

static void lifecycle_event(
    void *context, const MuseOnListenerLifecycleEvent *event) {
  TraceSink *sink = context;

  if (event->type == MUSE_ON_LISTENER_LIFECYCLE_EVENT_FOCUS_CHANGED) {
    sink->focus_count++;
    sink->foreground = event->foreground;
    assert(muse_on_topology_host_apply_focus_changed(
        &sink->host, kGeneration, event->foreground));
    apply_coordinator(sink, MUSE_ON_COMMAND_NONE);
    return;
  }
  if (event->type == MUSE_ON_LISTENER_LIFECYCLE_EVENT_NEUTRAL_ENTRY) {
    sink->neutral_count++;
    assert(muse_on_topology_host_apply_neutral_entry(
        &sink->host, kGeneration, event->inputs_released));
    apply_coordinator(sink, MUSE_ON_COMMAND_NONE);
    return;
  }
  assert(event->type == MUSE_ON_LISTENER_LIFECYCLE_EVENT_ACTION);
  assert(event->action.id == MUSE_ON_ACTION_GLOBAL_DICTATION_HOLD);
  if (event->action.phase == MUSE_ON_ACTION_BEGIN) {
    sink->begin_count++;
  } else {
    assert(event->action.phase == MUSE_ON_ACTION_END);
    sink->end_count++;
  }
}

static void initialize_trace(
    TraceSink *sink, MuseOnListenerLifecycle *lifecycle) {
  static const uint8_t keyboard_neutral[8] = {0};
  static const uint8_t joystick_neutral[12] = {
      0x01, 0x80, 0x80, 0x80, 0x80, 0x80,
      0x80, 0x80, 0xff, 0xff, 0x00, 0x00,
  };

  muse_on_topology_host_init(&sink->host);
  muse_on_state_init(&sink->coordinator);
  muse_on_topology_host_begin(&sink->host, kGeneration, false);
  assert(muse_on_topology_host_apply_topology(
             &sink->host, kGeneration,
             (MuseOnConnectionSnapshot){
                 MUSE_ON_CONNECTION_SINGLE, kControllerLocation}) ==
         MUSE_ON_TOPOLOGY_EVENT_ACCEPTED);
  assert(muse_on_topology_host_apply_filter_verification(
      &sink->host, kGeneration, true));
  muse_on_listener_lifecycle_init(lifecycle, lifecycle_event, sink);
  assert(muse_on_listener_lifecycle_bind(
      lifecycle, MUSE_ON_PROFILE_PEDAL, kControllerLocation, true, true));
  assert(muse_on_listener_lifecycle_focus_changed(lifecycle, true));
  assert(muse_on_listener_lifecycle_observe_report(
      lifecycle, MUSE_ON_INTERFACE_KEYBOARD_BOOT, 0, keyboard_neutral,
      sizeof(keyboard_neutral), UINT64_C(1000000000), false));
  assert(muse_on_listener_lifecycle_observe_report(
      lifecycle, MUSE_ON_INTERFACE_JOYSTICK, 1, joystick_neutral,
      sizeof(joystick_neutral), UINT64_C(1000000001), false));
  apply_coordinator(sink, MUSE_ON_COMMAND_ENABLE);
  assert(sink->coordinator.status == MUSE_ON_STATUS_ACTIVE);
}

static void test_captured_focus_epoch_trace(void) {
  static const uint8_t pedal_down[12] = {
      0x01, 0x80, 0x80, 0x80, 0x80, 0x80,
      0x80, 0x80, 0xff, 0xff, 0x10, 0x00,
  };
  static const uint8_t pedal_release[12] = {
      0x01, 0x80, 0x80, 0x80, 0x80, 0x80,
      0x80, 0x80, 0xff, 0xff, 0x00, 0x00,
  };
  TraceSink sink = {0};
  MuseOnListenerLifecycle lifecycle;
  const uint64_t start_ns = UINT64_C(2000000000);
  unsigned int neutral_before;

  initialize_trace(&sink, &lifecycle);
  assert(muse_on_listener_lifecycle_focus_changed(&lifecycle, false));
  assert(!sink.host.inputs_released);
  assert(sink.host.filter_verified);
  assert(sink.coordinator.status == MUSE_ON_STATUS_INACTIVE);
  assert(sink.coordinator.inactive_reason ==
         MUSE_ON_INACTIVE_REASON_NOT_FOREGROUND);
  assert(muse_on_listener_lifecycle_focus_changed(&lifecycle, true));
  assert(!sink.host.inputs_released);
  assert(sink.coordinator.status == MUSE_ON_STATUS_INACTIVE);
  assert(sink.coordinator.inactive_reason ==
         MUSE_ON_INACTIVE_REASON_RELEASE_CONTROLS);

  assert(muse_on_listener_lifecycle_observe_report(
      &lifecycle, MUSE_ON_INTERFACE_JOYSTICK, 1, pedal_down,
      sizeof(pedal_down), start_ns, true));
  assert(sink.begin_count == 0 && sink.end_count == 0);
  neutral_before = sink.neutral_count;
  assert(muse_on_listener_lifecycle_observe_report(
      &lifecycle, MUSE_ON_INTERFACE_JOYSTICK, 1, pedal_release,
      sizeof(pedal_release), start_ns + MUSE_ON_HOLD_DEBOUNCE_NS, true));
  assert(sink.neutral_count == neutral_before + 1);
  assert(sink.host.inputs_released);
  assert(sink.coordinator.status == MUSE_ON_STATUS_ACTIVE);
  assert(sink.begin_count == 0 && sink.end_count == 0);

  assert(muse_on_listener_lifecycle_observe_report(
      &lifecycle, MUSE_ON_INTERFACE_JOYSTICK, 1, pedal_down,
      sizeof(pedal_down), start_ns + 2 * MUSE_ON_HOLD_DEBOUNCE_NS, true));
  muse_on_listener_lifecycle_tick(
      &lifecycle, start_ns + 3 * MUSE_ON_HOLD_DEBOUNCE_NS, true);
  assert(sink.begin_count == 1 && sink.end_count == 0);
  assert(muse_on_listener_lifecycle_observe_report(
      &lifecycle, MUSE_ON_INTERFACE_JOYSTICK, 1, pedal_release,
      sizeof(pedal_release), start_ns + 4 * MUSE_ON_HOLD_DEBOUNCE_NS, true));
  muse_on_listener_lifecycle_tick(
      &lifecycle, start_ns + 5 * MUSE_ON_HOLD_DEBOUNCE_NS, true);
  assert(sink.begin_count == 1 && sink.end_count == 1);
}

static void test_held_across_focus_return_is_blocked(void) {
  static const uint8_t pedal_down[12] = {
      0x01, 0x80, 0x80, 0x80, 0x80, 0x80,
      0x80, 0x80, 0xff, 0xff, 0x10, 0x00,
  };
  static const uint8_t pedal_release[12] = {
      0x01, 0x80, 0x80, 0x80, 0x80, 0x80,
      0x80, 0x80, 0xff, 0xff, 0x00, 0x00,
  };
  TraceSink sink = {0};
  MuseOnListenerLifecycle lifecycle;
  const uint64_t start_ns = UINT64_C(3000000000);

  initialize_trace(&sink, &lifecycle);
  assert(muse_on_listener_lifecycle_focus_changed(&lifecycle, false));
  assert(muse_on_listener_lifecycle_observe_report(
      &lifecycle, MUSE_ON_INTERFACE_JOYSTICK, 1, pedal_down,
      sizeof(pedal_down), start_ns, true));
  assert(muse_on_listener_lifecycle_focus_changed(&lifecycle, true));
  muse_on_listener_lifecycle_tick(
      &lifecycle, start_ns + MUSE_ON_HOLD_DEBOUNCE_NS, true);
  assert(sink.begin_count == 0 && sink.end_count == 0);
  assert(!sink.host.inputs_released);
  assert(muse_on_listener_lifecycle_observe_report(
      &lifecycle, MUSE_ON_INTERFACE_JOYSTICK, 1, pedal_release,
      sizeof(pedal_release), start_ns + 2 * MUSE_ON_HOLD_DEBOUNCE_NS, true));
  assert(sink.host.inputs_released);
  assert(sink.begin_count == 0 && sink.end_count == 0);
}

int main(void) {
  test_captured_focus_epoch_trace();
  test_held_across_focus_return_is_blocked();
  return 0;
}
