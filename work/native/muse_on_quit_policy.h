#ifndef MUSE_ON_QUIT_POLICY_H
#define MUSE_ON_QUIT_POLICY_H

#include <stdbool.h>

typedef enum {
  MUSE_ON_QUIT_POLICY_NORMAL_SAFE_QUIT = 0,
  MUSE_ON_QUIT_POLICY_CONFIRM_UNCLEAN_QUIT,
  MUSE_ON_QUIT_POLICY_TERMINATE_UNCLEAN_QUIT,
  MUSE_ON_QUIT_POLICY_WAIT_FOR_IN_FLIGHT_QUIT
} MuseOnQuitPolicyDecision;

typedef struct {
  bool safety_latched;
  bool disable_pending;
  bool unclean_quit_authorized;
  bool quit_in_flight;
} MuseOnQuitPolicyInput;

/* Classify the quit request without changing state or performing I/O. */
MuseOnQuitPolicyDecision muse_on_quit_policy_decide(
    MuseOnQuitPolicyInput input);

#endif
