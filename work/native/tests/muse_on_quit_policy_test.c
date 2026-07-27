#include <assert.h>

#include "../muse_on_quit_policy.h"

static MuseOnQuitPolicyInput normal_input(void) {
  return (MuseOnQuitPolicyInput){
      .safety_latched = false,
      .disable_pending = false,
      .unclean_quit_authorized = false,
      .quit_in_flight = false,
  };
}

static void test_normal_safe_quit_boundary(void) {
  MuseOnQuitPolicyInput input = normal_input();

  assert(muse_on_quit_policy_decide(input) ==
         MUSE_ON_QUIT_POLICY_NORMAL_SAFE_QUIT);
  input.unclean_quit_authorized = true;
  assert(muse_on_quit_policy_decide(input) ==
         MUSE_ON_QUIT_POLICY_TERMINATE_UNCLEAN_QUIT);
}

static void test_latch_requires_exceptional_confirmation(void) {
  MuseOnQuitPolicyInput input = normal_input();

  input.safety_latched = true;
  assert(muse_on_quit_policy_decide(input) ==
         MUSE_ON_QUIT_POLICY_CONFIRM_UNCLEAN_QUIT);

  input.safety_latched = false;
  input.disable_pending = true;
  assert(muse_on_quit_policy_decide(input) ==
         MUSE_ON_QUIT_POLICY_CONFIRM_UNCLEAN_QUIT);
}

static void test_only_explicit_authorization_terminates_uncleanly(void) {
  MuseOnQuitPolicyInput input = {
      .safety_latched = true,
      .disable_pending = false,
      .unclean_quit_authorized = true,
  };

  assert(muse_on_quit_policy_decide(input) ==
         MUSE_ON_QUIT_POLICY_TERMINATE_UNCLEAN_QUIT);

  input.unclean_quit_authorized = false;
  assert(muse_on_quit_policy_decide(input) !=
         MUSE_ON_QUIT_POLICY_TERMINATE_UNCLEAN_QUIT);
}

static void test_routine_states_never_show_exceptional_quit_policy(void) {
  MuseOnQuitPolicyInput input = normal_input();

  assert(muse_on_quit_policy_decide(input) !=
         MUSE_ON_QUIT_POLICY_CONFIRM_UNCLEAN_QUIT);
  assert(muse_on_quit_policy_decide(input) !=
         MUSE_ON_QUIT_POLICY_TERMINATE_UNCLEAN_QUIT);
}

static void test_direct_quit_requires_confirmation_and_allows_one_attempt(void) {
  MuseOnQuitPolicyInput input = normal_input();

  input.safety_latched = true;
  assert(muse_on_quit_policy_decide(input) ==
         MUSE_ON_QUIT_POLICY_CONFIRM_UNCLEAN_QUIT);

  input.unclean_quit_authorized = true;
  assert(muse_on_quit_policy_decide(input) ==
         MUSE_ON_QUIT_POLICY_TERMINATE_UNCLEAN_QUIT);

  input.quit_in_flight = true;
  assert(muse_on_quit_policy_decide(input) ==
         MUSE_ON_QUIT_POLICY_WAIT_FOR_IN_FLIGHT_QUIT);
}

int main(void) {
  test_normal_safe_quit_boundary();
  test_latch_requires_exceptional_confirmation();
  test_only_explicit_authorization_terminates_uncleanly();
  test_routine_states_never_show_exceptional_quit_policy();
  test_direct_quit_requires_confirmation_and_allows_one_attempt();
  return 0;
}
