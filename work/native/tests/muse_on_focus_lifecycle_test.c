#include "../muse_on_capture.h"
#include "../muse_on_topology.h"

#include <assert.h>

typedef struct {
  MuseOnDecoder keyboard_decoder;
  MuseOnDecoder joystick_decoder;
  MuseOnActionRouter keyboard_router;
  MuseOnActionRouter joystick_router;
  bool keyboard_ready;
  bool joystick_ready;
} FocusInputs;

typedef struct {
  unsigned int begin_count;
  unsigned int end_count;
} ActionLog;

static const uint8_t kNeutralKeyboard[8] = {0};
static const uint8_t kNeutralJoystick[11] = {
    0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
    0x80, 0xff, 0xff, 0x00, 0x00,
};

static bool unavailable_current_report(
    void *context, uint8_t report_id, uint8_t *report,
    size_t *report_length) {
  (void)context;
  (void)report_id;
  (void)report;
  (void)report_length;
  return false;
}

static void init_inputs(FocusInputs *inputs) {
  MuseOnInputObservation observation;

  muse_on_decoder_init(&inputs->keyboard_decoder);
  muse_on_decoder_init(&inputs->joystick_decoder);
  muse_on_action_router_init(
      &inputs->keyboard_router, MUSE_ON_PROFILE_PEDAL);
  muse_on_action_router_init(
      &inputs->joystick_router, MUSE_ON_PROFILE_PEDAL);
  assert(muse_on_capture_observe_report(
      &inputs->keyboard_decoder, MUSE_ON_INTERFACE_KEYBOARD_BOOT,
      MUSE_ON_PROFILE_PEDAL, 0, kNeutralKeyboard, sizeof(kNeutralKeyboard),
      &observation));
  assert(observation.neutral_entry_state == MUSE_ON_NEUTRAL_ENTRY_RELEASED);
  assert(muse_on_capture_observe_report(
      &inputs->joystick_decoder, MUSE_ON_INTERFACE_JOYSTICK,
      MUSE_ON_PROFILE_PEDAL, 1, kNeutralJoystick, sizeof(kNeutralJoystick),
      &observation));
  assert(observation.neutral_entry_state == MUSE_ON_NEUTRAL_ENTRY_RELEASED);
  inputs->keyboard_ready = true;
  inputs->joystick_ready = true;
}

static bool inputs_ready(const FocusInputs *inputs) {
  return inputs->keyboard_ready && inputs->joystick_ready;
}

static void record_action(ActionLog *log, const MuseOnActionEvent *action) {
  assert(action->id == MUSE_ON_ACTION_GLOBAL_DICTATION_HOLD);
  if (action->phase == MUSE_ON_ACTION_BEGIN) {
    log->begin_count++;
  } else {
    assert(action->phase == MUSE_ON_ACTION_END);
    log->end_count++;
  }
}

static void process_joystick_report(
    FocusInputs *inputs, const uint8_t *report, bool foreground,
    uint64_t timestamp_ns, ActionLog *log) {
  MuseOnInputObservation observation;
  bool ready_before = inputs_ready(inputs);

  assert(muse_on_capture_observe_report(
      &inputs->joystick_decoder, MUSE_ON_INTERFACE_JOYSTICK,
      MUSE_ON_PROFILE_PEDAL, 1, report, 11, &observation));
  if (!foreground || !ready_before) {
    bool released =
        muse_on_capture_controller_neutral_state(
            &inputs->keyboard_decoder, &inputs->joystick_decoder,
            MUSE_ON_PROFILE_PEDAL) == MUSE_ON_NEUTRAL_ENTRY_RELEASED;
    inputs->keyboard_ready = released;
    inputs->joystick_ready = released;
    return;
  }
  for (size_t index = 0; index < observation.event_count; index++) {
    MuseOnActionEvent action;
    if (muse_on_action_router_route(
            &inputs->joystick_router, observation.events[index].name,
            timestamp_ns, &action)) {
      record_action(log, &action);
    }
  }
}

static void tick_joystick(
    FocusInputs *inputs, uint64_t timestamp_ns, ActionLog *log) {
  MuseOnActionEvent action;

  if (muse_on_action_router_tick(
          &inputs->joystick_router, timestamp_ns, &action)) {
    record_action(log, &action);
  }
}

static MuseOnTopologyCoordinatorInputs coordinator_inputs(bool foreground) {
  return (MuseOnTopologyCoordinatorInputs){
      .permission_granted = true,
      .session_available = true,
      .codex_foreground = foreground,
      .cleanup_verified = true,
  };
}

static void apply_coordinator(
    MuseOnTopologyHostState *host, MuseOnState *coordinator,
    MuseOnCommand command, bool foreground) {
  (void)muse_on_topology_host_apply_coordinator(
      host, coordinator, command, MUSE_ON_SAFETY_FAILURE_NONE,
      coordinator_inputs(foreground));
}

static void test_focus_epoch_consumes_release_before_dispatch(void) {
  FocusInputs inputs;
  MuseOnTopologyHostState host;
  MuseOnState coordinator;
  ActionLog log = {0};
  uint8_t pedal_down[11];
  uint8_t current_report[8] = {0};
  const uint64_t down_ns = UINT64_C(1000000000);

  init_inputs(&inputs);
  muse_on_topology_host_init(&host);
  muse_on_state_init(&coordinator);
  muse_on_topology_host_begin(&host, 1, false);
  assert(muse_on_topology_host_apply_topology(
             &host, 1,
             (MuseOnConnectionSnapshot){
                 MUSE_ON_CONNECTION_SINGLE, UINT32_C(0x110000)}) ==
         MUSE_ON_TOPOLOGY_EVENT_ACCEPTED);
  assert(muse_on_topology_host_apply_filter_verification(&host, 1, true));
  assert(muse_on_topology_host_apply_neutral_entry(&host, 1, true));
  apply_coordinator(&host, &coordinator, MUSE_ON_COMMAND_ENABLE, true);
  assert(coordinator.status == MUSE_ON_STATUS_ACTIVE);
  assert(coordinator.effects.request_dispatch);

  muse_on_capture_focus_changed(
      &inputs.keyboard_decoder, &inputs.keyboard_router,
      MUSE_ON_PROFILE_PEDAL, false, &inputs.keyboard_ready);
  muse_on_capture_focus_changed(
      &inputs.joystick_decoder, &inputs.joystick_router,
      MUSE_ON_PROFILE_PEDAL, false, &inputs.joystick_ready);
  assert(inputs.keyboard_decoder.keyboard_usages_seen);
  assert(inputs.joystick_decoder.joystick_report_seen);
  assert(!inputs_ready(&inputs));
  muse_on_topology_host_require_neutral_entry(&host);
  apply_coordinator(&host, &coordinator, MUSE_ON_COMMAND_NONE, false);
  assert(host.filter_verified);
  assert(coordinator.status == MUSE_ON_STATUS_INACTIVE);
  assert(coordinator.inactive_reason ==
         MUSE_ON_INACTIVE_REASON_NOT_FOREGROUND);
  assert(!coordinator.effects.request_dispatch);

  muse_on_capture_focus_changed(
      &inputs.keyboard_decoder, &inputs.keyboard_router,
      MUSE_ON_PROFILE_PEDAL, true, &inputs.keyboard_ready);
  muse_on_capture_focus_changed(
      &inputs.joystick_decoder, &inputs.joystick_router,
      MUSE_ON_PROFILE_PEDAL, true, &inputs.joystick_ready);
  assert(muse_on_capture_probe_neutral_entry(
             &inputs.keyboard_decoder, MUSE_ON_INTERFACE_KEYBOARD_BOOT,
             MUSE_ON_PROFILE_PEDAL, 0, current_report,
             sizeof(current_report), unavailable_current_report, NULL) ==
         MUSE_ON_NEUTRAL_ENTRY_UNKNOWN);
  assert(muse_on_capture_controller_neutral_state(
             &inputs.keyboard_decoder, &inputs.joystick_decoder,
             MUSE_ON_PROFILE_PEDAL) == MUSE_ON_NEUTRAL_ENTRY_RELEASED);
  apply_coordinator(&host, &coordinator, MUSE_ON_COMMAND_NONE, true);
  assert(coordinator.status == MUSE_ON_STATUS_INACTIVE);
  assert(coordinator.inactive_reason ==
         MUSE_ON_INACTIVE_REASON_RELEASE_CONTROLS);
  assert(!coordinator.effects.request_dispatch);

  for (size_t index = 0; index < sizeof(pedal_down); index++) {
    pedal_down[index] = kNeutralJoystick[index];
  }
  pedal_down[9] = 0x10;
  process_joystick_report(&inputs, pedal_down, true, down_ns, &log);
  assert(!inputs_ready(&inputs));
  assert(muse_on_topology_host_apply_neutral_entry(&host, 1, false));
  apply_coordinator(&host, &coordinator, MUSE_ON_COMMAND_NONE, true);
  assert(coordinator.status == MUSE_ON_STATUS_INACTIVE);
  assert(log.begin_count == 0);
  assert(log.end_count == 0);

  process_joystick_report(
      &inputs, kNeutralJoystick, true,
      down_ns + MUSE_ON_HOLD_DEBOUNCE_NS, &log);
  assert(inputs_ready(&inputs));
  assert(muse_on_topology_host_apply_neutral_entry(&host, 1, true));
  apply_coordinator(&host, &coordinator, MUSE_ON_COMMAND_NONE, true);
  assert(coordinator.status == MUSE_ON_STATUS_ACTIVE);
  assert(coordinator.effects.request_dispatch);
  assert(log.begin_count == 0);
  assert(log.end_count == 0);

  process_joystick_report(
      &inputs, pedal_down, true,
      down_ns + 2 * MUSE_ON_HOLD_DEBOUNCE_NS, &log);
  tick_joystick(
      &inputs, down_ns + 3 * MUSE_ON_HOLD_DEBOUNCE_NS, &log);
  assert(log.begin_count == 1);
  assert(log.end_count == 0);
  process_joystick_report(
      &inputs, kNeutralJoystick, true,
      down_ns + 4 * MUSE_ON_HOLD_DEBOUNCE_NS, &log);
  tick_joystick(
      &inputs, down_ns + 5 * MUSE_ON_HOLD_DEBOUNCE_NS, &log);
  assert(log.begin_count == 1);
  assert(log.end_count == 1);
}

static void test_control_held_across_focus_return_waits_for_release(void) {
  FocusInputs inputs;
  MuseOnDecoder unknown_keyboard;
  ActionLog log = {0};
  uint8_t pedal_down[11];
  const uint64_t down_ns = UINT64_C(2000000000);

  init_inputs(&inputs);
  muse_on_decoder_init(&unknown_keyboard);
  assert(muse_on_capture_controller_neutral_state(
             &unknown_keyboard, &inputs.joystick_decoder,
             MUSE_ON_PROFILE_PEDAL) == MUSE_ON_NEUTRAL_ENTRY_UNKNOWN);
  for (size_t index = 0; index < sizeof(pedal_down); index++) {
    pedal_down[index] = kNeutralJoystick[index];
  }
  pedal_down[9] = 0x10;

  muse_on_capture_focus_changed(
      &inputs.keyboard_decoder, &inputs.keyboard_router,
      MUSE_ON_PROFILE_PEDAL, false, &inputs.keyboard_ready);
  muse_on_capture_focus_changed(
      &inputs.joystick_decoder, &inputs.joystick_router,
      MUSE_ON_PROFILE_PEDAL, false, &inputs.joystick_ready);
  process_joystick_report(
      &inputs, pedal_down, false, down_ns, &log);
  assert(log.begin_count == 0);
  assert(log.end_count == 0);
  muse_on_capture_focus_changed(
      &inputs.keyboard_decoder, &inputs.keyboard_router,
      MUSE_ON_PROFILE_PEDAL, true, &inputs.keyboard_ready);
  muse_on_capture_focus_changed(
      &inputs.joystick_decoder, &inputs.joystick_router,
      MUSE_ON_PROFILE_PEDAL, true, &inputs.joystick_ready);
  assert(muse_on_capture_controller_neutral_state(
             &inputs.keyboard_decoder, &inputs.joystick_decoder,
             MUSE_ON_PROFILE_PEDAL) == MUSE_ON_NEUTRAL_ENTRY_HELD);
  assert(!inputs_ready(&inputs));
  tick_joystick(
      &inputs, down_ns + 2 * MUSE_ON_HOLD_DEBOUNCE_NS, &log);
  assert(log.begin_count == 0);
  assert(log.end_count == 0);

  process_joystick_report(
      &inputs, kNeutralJoystick, true,
      down_ns + 3 * MUSE_ON_HOLD_DEBOUNCE_NS, &log);
  assert(inputs_ready(&inputs));
  assert(log.begin_count == 0);
  assert(log.end_count == 0);
}

int main(void) {
  test_focus_epoch_consumes_release_before_dispatch();
  test_control_held_across_focus_return_waits_for_release();
  return 0;
}
