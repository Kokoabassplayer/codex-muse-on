#include <assert.h>

#include "../muse_on_activation.h"

static void test_capture_and_dispatch_are_fail_closed(void) {
  MuseOnMode invalid_mode = (MuseOnMode)999;

  assert(!muse_on_should_filter_keyboard(MUSE_ON_MODE_DRY_RUN));
  assert(muse_on_should_filter_keyboard(MUSE_ON_MODE_CAPTURE_DRY_RUN));
  assert(muse_on_should_filter_keyboard(MUSE_ON_MODE_ACTIVE));

  assert(muse_on_can_route_actions(MUSE_ON_MODE_DRY_RUN, false, false));
  assert(!muse_on_can_route_actions(MUSE_ON_MODE_CAPTURE_DRY_RUN, true, false));
  assert(muse_on_can_route_actions(MUSE_ON_MODE_CAPTURE_DRY_RUN, true, true));
  assert(!muse_on_can_route_actions(MUSE_ON_MODE_ACTIVE, false, true));

  assert(!muse_on_can_post_actions(MUSE_ON_MODE_DRY_RUN, true, true, true));
  assert(!muse_on_can_post_actions(MUSE_ON_MODE_CAPTURE_DRY_RUN,
                                   true, true, true));
  assert(!muse_on_can_post_actions(MUSE_ON_MODE_ACTIVE, true, false, true));
  assert(!muse_on_can_post_actions(MUSE_ON_MODE_ACTIVE, true, true, false));
  assert(muse_on_can_post_actions(MUSE_ON_MODE_ACTIVE, true, true, true));

  assert(!muse_on_should_filter_keyboard(invalid_mode));
  assert(!muse_on_can_route_actions(invalid_mode, true, true));
  assert(!muse_on_can_post_actions(invalid_mode, true, true, true));
}

int main(void) {
  test_capture_and_dispatch_are_fail_closed();
  return 0;
}
