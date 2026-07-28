#ifndef MUSE_ON_KEY_FILTER_H
#define MUSE_ON_KEY_FILTER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct MuseOnKeyFilter MuseOnKeyFilter;

typedef struct {
  uint64_t source;
  uint64_t destination;
} MuseOnKeyFilterMapping;

/*
 * IOHIDEventSystem can retain a just-unplugged keyboard service briefly after
 * IOHIDManager reports the interface removal. Keep dispatch blocked while
 * bounded restore attempts wait for that service teardown to settle.
 */
#define MUSE_ON_KEY_FILTER_RESTORE_SETTLEMENT_NS UINT64_C(2000000000)

typedef enum {
  MUSE_ON_KEY_FILTER_RESTORE_RETRY = 0,
  MUSE_ON_KEY_FILTER_RESTORE_COMPLETE,
  MUSE_ON_KEY_FILTER_RESTORE_FAILED,
} MuseOnKeyFilterRestoreDecision;

typedef struct {
  bool waiting;
  uint64_t started_at_ns;
} MuseOnKeyFilterRestorePolicy;

extern const MuseOnKeyFilterMapping muse_on_key_filter_mappings[];
extern const size_t muse_on_key_filter_mapping_count;

/* Pure seams for mapping-table and service-selection tests. */
bool muse_on_key_filter_mapping_table_is_valid(void);
bool muse_on_key_filter_service_matches(uint32_t vendor_id, uint32_t product_id,
                                        uint32_t usage_page, uint32_t usage,
                                        uint64_t location_id,
                                        uint64_t requested_location_id);
bool muse_on_key_filter_restore_is_verified(bool original_mapping_was_null,
                                            bool readback_is_null,
                                            size_t readback_count);
void muse_on_key_filter_restore_policy_init(
    MuseOnKeyFilterRestorePolicy *policy);
MuseOnKeyFilterRestoreDecision muse_on_key_filter_restore_policy_evaluate(
    MuseOnKeyFilterRestorePolicy *policy, uint64_t now_ns,
    bool restore_succeeded, bool keyboard_present, bool final_attempt);

MuseOnKeyFilter *muse_on_key_filter_create(void);
void muse_on_key_filter_destroy(MuseOnKeyFilter *filter);
bool muse_on_key_filter_apply(MuseOnKeyFilter *filter,
                              uint64_t requested_location_id);
bool muse_on_key_filter_restore(MuseOnKeyFilter *filter);
bool muse_on_key_filter_is_active(const MuseOnKeyFilter *filter);
bool muse_on_key_filter_needs_restore(const MuseOnKeyFilter *filter);
uint64_t muse_on_key_filter_location_id(const MuseOnKeyFilter *filter);

#endif
