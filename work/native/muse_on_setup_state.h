#ifndef MUSE_ON_SETUP_STATE_H
#define MUSE_ON_SETUP_STATE_H

#include <stdbool.h>

/*
 * Persisted, user-owned setup choices.  This tiny pure-C boundary makes the
 * First Enable flow testable without invoking AppKit, TCC, or SMAppService.
 */
typedef struct {
  bool enabled;
  bool start_automatically;
} MuseOnSetupState;

void muse_on_setup_state_init(MuseOnSetupState *state, bool enabled,
                              bool start_automatically);
void muse_on_setup_state_confirm_first_enable(MuseOnSetupState *state);
void muse_on_setup_state_disable(MuseOnSetupState *state);
void muse_on_setup_state_set_start_automatically(MuseOnSetupState *state,
                                                  bool enabled);

#endif
