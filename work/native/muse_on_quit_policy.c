#include "muse_on_quit_policy.h"

MuseOnQuitPolicyDecision muse_on_quit_policy_decide(
    MuseOnQuitPolicyInput input) {
  if (input.unclean_quit_authorized) {
    return MUSE_ON_QUIT_POLICY_TERMINATE_UNCLEAN_QUIT;
  }
  if (!input.safety_latched && !input.disable_pending) {
    return MUSE_ON_QUIT_POLICY_NORMAL_SAFE_QUIT;
  }
  return MUSE_ON_QUIT_POLICY_CONFIRM_UNCLEAN_QUIT;
}
