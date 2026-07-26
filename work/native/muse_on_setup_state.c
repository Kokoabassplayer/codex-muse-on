#include "muse_on_setup_state.h"

void muse_on_setup_state_init(MuseOnSetupState *state, bool enabled,
                              bool start_automatically) {
  if (state == 0) return;
  state->enabled = enabled;
  state->start_automatically = start_automatically;
}

void muse_on_setup_state_confirm_first_enable(MuseOnSetupState *state) {
  if (state == 0) return;
  state->enabled = true;
  /* The approved default is on after First Enable. */
  state->start_automatically = true;
}

void muse_on_setup_state_disable(MuseOnSetupState *state) {
  if (state == 0) return;
  state->enabled = false;
}

void muse_on_setup_state_set_start_automatically(MuseOnSetupState *state,
                                                  bool enabled) {
  if (state == 0) return;
  state->start_automatically = enabled;
}
