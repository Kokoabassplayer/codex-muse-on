#include "muse_on_config.h"

#include <string.h>

bool muse_on_config_parse(int argc, const char *const argv[],
                          MuseOnConfig *config) {
  MuseOnConfig candidate;
  bool mode_seen = false;
  bool profile_seen = false;
  bool safety_seen = false;
  int index;

  if (!config || argc < 1 || !argv) return false;
  candidate.profile = MUSE_ON_PROFILE_CONTROLLER_ONLY;
  candidate.mode = MUSE_ON_MODE_DRY_RUN;
  candidate.safety_latched = false;

  for (index = 1; index < argc; index++) {
    const char *argument = argv[index];
    if (!argument) return false;

    if (strcmp(argument, "--profile=controller-only") == 0 ||
        strcmp(argument, "--profile=pedal") == 0) {
      if (profile_seen) return false;
      profile_seen = true;
      candidate.profile = strcmp(argument, "--profile=pedal") == 0
          ? MUSE_ON_PROFILE_PEDAL
          : MUSE_ON_PROFILE_CONTROLLER_ONLY;
      continue;
    }

    if (strcmp(argument, "--mode=dry-run") == 0 ||
        strcmp(argument, "--mode=capture-dry-run") == 0 ||
        strcmp(argument, "--mode=active") == 0) {
      if (mode_seen) return false;
      mode_seen = true;
      if (strcmp(argument, "--mode=active") == 0) {
        candidate.mode = MUSE_ON_MODE_ACTIVE;
      } else if (strcmp(argument, "--mode=capture-dry-run") == 0) {
        candidate.mode = MUSE_ON_MODE_CAPTURE_DRY_RUN;
      }
      continue;
    }

    if (strcmp(argument, "--safety-latched") == 0) {
      if (safety_seen) return false;
      safety_seen = true;
      candidate.safety_latched = true;
      continue;
    }

    return false;
  }
  *config = candidate;
  return true;
}

const char *muse_on_mode_string(MuseOnMode mode) {
  switch (mode) {
    case MUSE_ON_MODE_DRY_RUN: return "dry_run";
    case MUSE_ON_MODE_CAPTURE_DRY_RUN: return "capture_dry_run";
    case MUSE_ON_MODE_ACTIVE: return "active";
  }
  return "unknown";
}
