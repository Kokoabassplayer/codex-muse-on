#ifndef MUSE_ON_PLATFORM_H
#define MUSE_ON_PLATFORM_H

#include <stdbool.h>
#include <stdint.h>

#include "muse_on_shortcut_map.h"

typedef enum {
  MUSE_ON_POST_HYPER_KEY_TAP = 0,
  MUSE_ON_POST_HYPER_KEY_DOWN,
  MUSE_ON_POST_HYPER_KEY_UP
} MuseOnPostHyperKeyPhase;

typedef struct {
  uint16_t mac_virtual_key;
  bool key_down;
  uint64_t flags;
} MuseOnHyperKeyEvent;

typedef enum {
  MUSE_ON_PERMISSION_GATE_NONE = 0,
  MUSE_ON_PERMISSION_GATE_INPUT_MONITORING = 1 << 0,
  MUSE_ON_PERMISSION_GATE_ACCESSIBILITY = 1 << 1
} MuseOnPermissionGate;

typedef enum {
  MUSE_ON_PERMISSION_REQUEST_PASSIVE = 0,
  MUSE_ON_PERMISSION_REQUEST_FIRST_ENABLE,
  MUSE_ON_PERMISSION_REQUEST_RETRY
} MuseOnPermissionRequestTrigger;

typedef struct {
  MuseOnPermissionGate request_gates;
  bool mark_handled;
} MuseOnResponsiblePermissionPlan;

bool muse_on_bundle_id_is_codex(const char *bundle_id);
bool muse_on_codex_is_frontmost(void);
bool muse_on_session_is_available(void);
bool muse_on_preflight_post_event_access(void);
bool muse_on_request_post_event_access(void);
bool muse_on_input_monitoring_access_granted(void);
bool muse_on_input_monitoring_access_unknown(void);
bool muse_on_request_input_monitoring_access(void);
bool muse_on_should_launch_listener_probe(bool enabled, bool safety_latched,
                                          bool listener_running);
MuseOnPermissionGate muse_on_listener_missing_permission_gates(
    const char *input_monitoring, bool accessibility_granted);
bool muse_on_should_open_retry_permission_settings(
    bool retry_requested, bool authoritative, MuseOnPermissionGate missing,
    bool destination_already_opened);
MuseOnResponsiblePermissionPlan muse_on_responsible_permission_plan(
    MuseOnPermissionRequestTrigger trigger, bool enabled,
    bool request_already_attempted,
    bool input_monitoring_granted, bool accessibility_granted);
MuseOnPermissionGate muse_on_missing_permission_gates(
    bool input_monitoring_granted, bool accessibility_granted);
MuseOnPermissionGate muse_on_retry_permission_gate(
    bool input_monitoring_granted, bool accessibility_granted);
const char *muse_on_permission_guidance(MuseOnPermissionGate gates);
const char *muse_on_permission_gate_settings_url(MuseOnPermissionGate gate);
const char *muse_on_permission_fallback_settings_url(void);
bool muse_on_listener_error_is_permission_required(const char *operation,
                                                  int32_t code);
size_t muse_on_hyper_key_event_sequence(uint16_t mac_virtual_key,
                                        MuseOnPostHyperKeyPhase phase,
                                        MuseOnHyperKeyEvent *events,
                                        size_t capacity);
bool muse_on_post_hyper_key(uint16_t mac_virtual_key,
                            MuseOnPostHyperKeyPhase phase);
bool muse_on_mac_virtual_key(MuseOnShortcutKey key, uint16_t *mac_virtual_key);
bool muse_on_post_shortcut(const MuseOnShortcutInstruction *shortcut);

#endif
