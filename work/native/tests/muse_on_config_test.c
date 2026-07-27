#include <assert.h>
#include <string.h>

#include "../muse_on_config.h"

static void test_defaults_and_explicit_flags(void) {
  MuseOnConfig config;
  const char *defaults[] = {"muse_on_listener"};
  const char *explicit_flags[] = {
      "muse_on_listener", "--mode=active", "--profile=pedal",
      "--safety-latched"};

  assert(muse_on_config_parse(1, defaults, &config));
  assert(config.profile == MUSE_ON_PROFILE_CONTROLLER_ONLY);
  assert(config.mode == MUSE_ON_MODE_DRY_RUN);
  assert(config.request_permissions == false);

  assert(muse_on_config_parse(4, explicit_flags, &config));
  assert(config.profile == MUSE_ON_PROFILE_PEDAL);
  assert(config.mode == MUSE_ON_MODE_ACTIVE);
  assert(config.safety_latched == true);
  assert(config.request_permissions == false);
  assert(strcmp(muse_on_mode_string(config.mode), "active") == 0);

  {
    const char *request[] = {
        "muse_on_listener", "--mode=active", "--request-permissions"};
    assert(muse_on_config_parse(3, request, &config));
    assert(config.request_permissions == true);
  }
}

static void test_invalid_flags_fail_without_mutating_config(void) {
  MuseOnConfig config = {MUSE_ON_PROFILE_PEDAL, MUSE_ON_MODE_ACTIVE, false,
                         false};
  const char *unknown[] = {"muse_on_listener", "--mode=surprise"};

  assert(!muse_on_config_parse(2, unknown, &config));
  assert(config.profile == MUSE_ON_PROFILE_PEDAL);
  assert(config.mode == MUSE_ON_MODE_ACTIVE);

  {
    const char *request_without_active[] = {
        "muse_on_listener", "--request-permissions"};
    assert(!muse_on_config_parse(2, request_without_active, &config));
  }
}

int main(void) {
  test_defaults_and_explicit_flags();
  test_invalid_flags_fail_without_mutating_config();
  return 0;
}
