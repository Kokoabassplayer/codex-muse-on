#ifndef MUSE_ON_CONFIG_H
#define MUSE_ON_CONFIG_H

#include <stdbool.h>

#include "muse_on_action_map.h"

typedef enum {
  MUSE_ON_MODE_DRY_RUN = 0,
  MUSE_ON_MODE_CAPTURE_DRY_RUN,
  MUSE_ON_MODE_ACTIVE
} MuseOnMode;

typedef struct {
  MuseOnProfile profile;
  MuseOnMode mode;
  bool safety_latched; /* recovery mode: never dispatch actions */
} MuseOnConfig;

bool muse_on_config_parse(int argc, const char *const argv[],
                          MuseOnConfig *config);
const char *muse_on_mode_string(MuseOnMode mode);

#endif
