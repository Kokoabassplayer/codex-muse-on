#ifndef MUSE_ON_LISTENER_LIFECYCLE_H
#define MUSE_ON_LISTENER_LIFECYCLE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "muse_on_capture.h"

typedef enum {
  MUSE_ON_LISTENER_LIFECYCLE_EVENT_FOCUS_CHANGED = 0,
  MUSE_ON_LISTENER_LIFECYCLE_EVENT_NEUTRAL_ENTRY,
  MUSE_ON_LISTENER_LIFECYCLE_EVENT_ACTION,
} MuseOnListenerLifecycleEventType;

typedef struct {
  MuseOnListenerLifecycleEventType type;
  bool foreground;
  bool inputs_released;
  uint32_t controller_location_id;
  MuseOnInterfaceKind interface_kind;
  MuseOnActionEvent action;
} MuseOnListenerLifecycleEvent;

typedef void (*MuseOnListenerLifecycleSink)(
    void *context, const MuseOnListenerLifecycleEvent *event);

typedef enum {
  MUSE_ON_LISTENER_ROUTE_REQUIRES_FOREGROUND = 0,
  MUSE_ON_LISTENER_ROUTE_DRY_RUN,
} MuseOnListenerRoutePolicy;

typedef struct {
  MuseOnProfile profile;
  MuseOnListenerRoutePolicy route_policy;
  uint32_t controller_location_id;
  bool bound;
  bool focus_known;
  bool foreground;
  bool inputs_released;
  MuseOnDecoder keyboard_decoder;
  MuseOnDecoder joystick_decoder;
  MuseOnActionRouter keyboard_router;
  MuseOnActionRouter joystick_router;
  MuseOnListenerLifecycleSink sink;
  void *sink_context;
} MuseOnListenerLifecycle;

void muse_on_listener_lifecycle_init(
    MuseOnListenerLifecycle *lifecycle, MuseOnListenerRoutePolicy route_policy,
    MuseOnListenerLifecycleSink sink, void *sink_context);
bool muse_on_listener_lifecycle_bind(
    MuseOnListenerLifecycle *lifecycle, MuseOnProfile profile,
    uint32_t controller_location_id);
bool muse_on_listener_lifecycle_unbind(MuseOnListenerLifecycle *lifecycle);
bool muse_on_listener_lifecycle_focus_changed(
    MuseOnListenerLifecycle *lifecycle, bool foreground);
bool muse_on_listener_lifecycle_observe_report(
    MuseOnListenerLifecycle *lifecycle, MuseOnInterfaceKind interface_kind,
    uint8_t report_id, const uint8_t *report, size_t report_length,
    uint64_t timestamp_ns, bool external_route_gate);
void muse_on_listener_lifecycle_tick(
    MuseOnListenerLifecycle *lifecycle, uint64_t timestamp_ns,
    bool external_route_gate);
bool muse_on_listener_lifecycle_inputs_released(
    const MuseOnListenerLifecycle *lifecycle);
bool muse_on_listener_lifecycle_is_foreground(
    const MuseOnListenerLifecycle *lifecycle);
bool muse_on_listener_lifecycle_is_bound_to(
    const MuseOnListenerLifecycle *lifecycle, MuseOnProfile profile,
    uint32_t controller_location_id);

#endif
