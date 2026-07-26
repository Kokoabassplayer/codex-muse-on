#include <assert.h>

#include "../muse_on_setup_state.h"

static void test_first_enable_persists_enabled_and_starts_automatically(void) {
  MuseOnSetupState state;
  muse_on_setup_state_init(&state, false, false);

  muse_on_setup_state_confirm_first_enable(&state);

  assert(state.enabled);
  assert(state.start_automatically);
}

static void test_cancel_is_a_strict_no_op(void) {
  MuseOnSetupState state;
  muse_on_setup_state_init(&state, false, false);

  /* A cancelled alert deliberately calls no transition. */
  assert(!state.enabled);
  assert(!state.start_automatically);
}

static void test_disable_keeps_startup_preference_independent(void) {
  MuseOnSetupState state;
  muse_on_setup_state_init(&state, true, true);

  muse_on_setup_state_disable(&state);

  assert(!state.enabled);
  assert(state.start_automatically);
}

int main(void) {
  test_first_enable_persists_enabled_and_starts_automatically();
  test_cancel_is_a_strict_no_op();
  test_disable_keeps_startup_preference_independent();
  return 0;
}
