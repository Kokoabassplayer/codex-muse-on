#include "muse_on_key_filter.h"

#include <stdlib.h>

#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/hid/IOHIDDeviceKeys.h>
#include <IOKit/hid/IOHIDProperties.h>
#include <IOKit/hid/IOHIDUsageTables.h>
#include <IOKit/hidsystem/IOHIDEventSystemClient.h>
#include <IOKit/hidsystem/IOHIDParameter.h>
#include <IOKit/hidsystem/IOHIDServiceClient.h>

enum {
  kMuseOnVendorID = 0x04b4,
  kMuseOnProductID = 0xe106,
  kMuseOnKeyboardUsagePage = 0x01,
  kMuseOnKeyboardUsage = 0x06,
};

struct MuseOnKeyFilter {
  IOHIDEventSystemClientRef client;
  CFTypeRef original_mapping;
  uint64_t active_location_id;
  uint64_t active_registry_id;
  bool original_mapping_was_null;
  bool restore_pending;
  bool active;
};

const MuseOnKeyFilterMapping muse_on_key_filter_mappings[] = {
    {UINT64_C(0x700000027), UINT64_C(0x700000000)},
    {UINT64_C(0x700000026), UINT64_C(0x700000000)},
    {UINT64_C(0x700000050), UINT64_C(0x700000000)},
    {UINT64_C(0x70000004f), UINT64_C(0x700000000)},
    {UINT64_C(0x70000003d), UINT64_C(0x700000000)},
    {UINT64_C(0x70000003c), UINT64_C(0x700000000)},
    {UINT64_C(0x70000001e), UINT64_C(0x700000000)},
    {UINT64_C(0x70000001f), UINT64_C(0x700000000)},
    {UINT64_C(0x700000020), UINT64_C(0x700000000)},
    {UINT64_C(0x7000000e1), UINT64_C(0x700000000)},
};

const size_t muse_on_key_filter_mapping_count =
    sizeof(muse_on_key_filter_mappings) / sizeof(muse_on_key_filter_mappings[0]);

bool muse_on_key_filter_mapping_table_is_valid(void) {
  static const uint64_t expected_sources[] = {
      UINT64_C(0x700000027), UINT64_C(0x700000026),
      UINT64_C(0x700000050), UINT64_C(0x70000004f),
      UINT64_C(0x70000003d), UINT64_C(0x70000003c),
      UINT64_C(0x70000001e), UINT64_C(0x70000001f),
      UINT64_C(0x700000020), UINT64_C(0x7000000e1),
  };
  size_t index;

  if (muse_on_key_filter_mapping_count !=
      sizeof(expected_sources) / sizeof(expected_sources[0])) {
    return false;
  }
  for (index = 0; index < muse_on_key_filter_mapping_count; ++index) {
    if (muse_on_key_filter_mappings[index].source != expected_sources[index] ||
        muse_on_key_filter_mappings[index].destination !=
            UINT64_C(0x700000000)) {
      return false;
    }
  }
  return true;
}

bool muse_on_key_filter_service_matches(uint32_t vendor_id, uint32_t product_id,
                                        uint32_t usage_page, uint32_t usage,
                                        uint64_t location_id,
                                        uint64_t requested_location_id) {
  return vendor_id == kMuseOnVendorID && product_id == kMuseOnProductID &&
         usage_page == kMuseOnKeyboardUsagePage &&
         usage == kMuseOnKeyboardUsage &&
         requested_location_id != 0 &&
         location_id == requested_location_id;
}

bool muse_on_key_filter_restore_target_matches(uint64_t saved_registry_id,
                                               uint64_t candidate_registry_id) {
  return saved_registry_id != 0 && saved_registry_id == candidate_registry_id;
}

MuseOnKeyFilterLookupResult muse_on_key_filter_select_service(
    const MuseOnKeyFilterServiceObservation *observations,
    size_t observation_count, uint64_t requested_registry_id,
    uint64_t requested_location_id, size_t *matched_index) {
  bool uncertain = false;
  size_t found_index = SIZE_MAX;
  size_t index;

  if (!matched_index) return MUSE_ON_KEY_FILTER_LOOKUP_UNCERTAIN;
  *matched_index = SIZE_MAX;
  if ((!observations && observation_count != 0) ||
      requested_registry_id == 0 || requested_location_id == 0) {
    return MUSE_ON_KEY_FILTER_LOOKUP_UNCERTAIN;
  }
  for (index = 0; index < observation_count; ++index) {
    const MuseOnKeyFilterServiceObservation *observation =
        &observations[index];

    if (!observation->registry_id_readable) {
      uncertain = true;
      continue;
    }
    if (!muse_on_key_filter_restore_target_matches(
            requested_registry_id, observation->registry_id)) {
      continue;
    }
    if (!observation->metadata_readable ||
        !muse_on_key_filter_service_matches(
            observation->vendor_id, observation->product_id,
            observation->usage_page, observation->usage,
            observation->location_id, requested_location_id)) {
      uncertain = true;
      continue;
    }
    if (found_index != SIZE_MAX) {
      return MUSE_ON_KEY_FILTER_LOOKUP_UNCERTAIN;
    }
    found_index = index;
  }
  if (found_index != SIZE_MAX) {
    *matched_index = found_index;
    return MUSE_ON_KEY_FILTER_LOOKUP_MATCHED;
  }
  return uncertain ? MUSE_ON_KEY_FILTER_LOOKUP_UNCERTAIN
                   : MUSE_ON_KEY_FILTER_LOOKUP_CONFIRMED_ABSENT;
}

bool muse_on_key_filter_restore_is_verified(bool original_mapping_was_null,
                                            bool readback_is_null,
                                            size_t readback_count) {
  if (original_mapping_was_null) {
    return readback_is_null || readback_count == 0;
  }
  return !readback_is_null;
}

void muse_on_key_filter_restore_policy_init(
    MuseOnKeyFilterRestorePolicy *policy) {
  if (!policy) return;
  policy->waiting = false;
  policy->started_at_ns = 0;
}

MuseOnKeyFilterRestoreDecision muse_on_key_filter_restore_policy_evaluate(
    MuseOnKeyFilterRestorePolicy *policy, uint64_t now_ns,
    bool restore_succeeded, bool keyboard_present, bool final_attempt) {
  uint64_t elapsed_ns;

  if (!policy) return MUSE_ON_KEY_FILTER_RESTORE_FAILED;
  if (restore_succeeded) {
    muse_on_key_filter_restore_policy_init(policy);
    return MUSE_ON_KEY_FILTER_RESTORE_COMPLETE;
  }
  if (final_attempt) {
    muse_on_key_filter_restore_policy_init(policy);
    return MUSE_ON_KEY_FILTER_RESTORE_FAILED;
  }
  if (!keyboard_present) {
    muse_on_key_filter_restore_policy_init(policy);
    return MUSE_ON_KEY_FILTER_RESTORE_RETRY;
  }
  if (!policy->waiting) {
    policy->waiting = true;
    policy->started_at_ns = now_ns;
    return MUSE_ON_KEY_FILTER_RESTORE_RETRY;
  }
  if (now_ns < policy->started_at_ns) {
    return MUSE_ON_KEY_FILTER_RESTORE_FAILED;
  }
  elapsed_ns = now_ns - policy->started_at_ns;
  if (elapsed_ns >= MUSE_ON_KEY_FILTER_RESTORE_SETTLEMENT_NS) {
    return MUSE_ON_KEY_FILTER_RESTORE_FAILED;
  }
  return MUSE_ON_KEY_FILTER_RESTORE_RETRY;
}

static bool copy_uint64_property(IOHIDServiceClientRef service, CFStringRef key,
                                 uint64_t *value) {
  CFTypeRef property;
  int64_t signed_value;
  bool copied = false;

  if (!service || !key || !value) return false;
  property = IOHIDServiceClientCopyProperty(service, key);
  if (property && CFGetTypeID(property) == CFNumberGetTypeID() &&
      CFNumberGetValue((CFNumberRef)property, kCFNumberSInt64Type,
                       &signed_value) && signed_value >= 0) {
    *value = (uint64_t)signed_value;
    copied = true;
  }
  if (property) CFRelease(property);
  return copied;
}

static bool copy_registry_id(IOHIDServiceClientRef service, uint64_t *value) {
  CFTypeRef registry_id;
  int64_t signed_value;

  if (!service || !value) return false;
  registry_id = IOHIDServiceClientGetRegistryID(service);
  if (!registry_id || CFGetTypeID(registry_id) != CFNumberGetTypeID() ||
      !CFNumberGetValue((CFNumberRef)registry_id, kCFNumberSInt64Type,
                        &signed_value) ||
      signed_value <= 0) {
    return false;
  }
  *value = (uint64_t)signed_value;
  return true;
}

static IOHIDServiceClientRef find_keyboard_service(
    IOHIDEventSystemClientRef client, uint64_t requested_location_id,
    uint64_t requested_registry_id, uint64_t *matched_location_id,
    uint64_t *matched_registry_id, MuseOnKeyFilterLookupResult *lookup_result) {
  CFArrayRef services;
  MuseOnKeyFilterServiceObservation *observations = NULL;
  size_t matched_index = SIZE_MAX;
  size_t service_count;
  CFIndex index;
  IOHIDServiceClientRef match = NULL;

  if (!client || !matched_location_id || !matched_registry_id ||
      !lookup_result || requested_location_id == 0 ||
      requested_registry_id == 0) {
    return NULL;
  }
  *lookup_result = MUSE_ON_KEY_FILTER_LOOKUP_UNCERTAIN;
  services = IOHIDEventSystemClientCopyServices(client);
  if (!services) return NULL;
  service_count = (size_t)CFArrayGetCount(services);
  if (service_count == 0) {
    *lookup_result = MUSE_ON_KEY_FILTER_LOOKUP_CONFIRMED_ABSENT;
    CFRelease(services);
    return NULL;
  }
  observations = calloc(service_count, sizeof(*observations));
  if (!observations) {
    CFRelease(services);
    return NULL;
  }
  for (index = 0; index < CFArrayGetCount(services); ++index) {
    IOHIDServiceClientRef service =
        (IOHIDServiceClientRef)CFArrayGetValueAtIndex(services, index);
    MuseOnKeyFilterServiceObservation *observation = &observations[index];
    uint64_t vendor_id;
    uint64_t product_id;
    uint64_t usage_page;
    uint64_t usage;
    uint64_t location_id;
    uint64_t registry_id;

    if (!service) {
      continue;
    }
    observation->registry_id_readable =
        copy_registry_id(service, &registry_id);
    if (observation->registry_id_readable) {
      observation->registry_id = registry_id;
    }
    observation->metadata_readable =
        copy_uint64_property(service, CFSTR(kIOHIDVendorIDKey), &vendor_id) &&
        copy_uint64_property(service, CFSTR(kIOHIDProductIDKey), &product_id) &&
        copy_uint64_property(service, CFSTR(kIOHIDPrimaryUsagePageKey),
                             &usage_page) &&
        copy_uint64_property(service, CFSTR(kIOHIDPrimaryUsageKey), &usage) &&
        copy_uint64_property(service, CFSTR(kIOHIDLocationIDKey), &location_id) &&
        vendor_id <= UINT32_MAX && product_id <= UINT32_MAX &&
        usage_page <= UINT32_MAX && usage <= UINT32_MAX;
    if (observation->metadata_readable) {
      observation->vendor_id = (uint32_t)vendor_id;
      observation->product_id = (uint32_t)product_id;
      observation->usage_page = (uint32_t)usage_page;
      observation->usage = (uint32_t)usage;
      observation->location_id = location_id;
    }
  }
  *lookup_result = muse_on_key_filter_select_service(
      observations, service_count, requested_registry_id,
      requested_location_id, &matched_index);
  if (*lookup_result == MUSE_ON_KEY_FILTER_LOOKUP_MATCHED &&
      matched_index < service_count) {
    IOHIDServiceClientRef service =
        (IOHIDServiceClientRef)CFArrayGetValueAtIndex(
            services, (CFIndex)matched_index);
    match = service ? (IOHIDServiceClientRef)CFRetain(service) : NULL;
    if (!match) {
      *lookup_result = MUSE_ON_KEY_FILTER_LOOKUP_UNCERTAIN;
    } else {
      *matched_location_id = observations[matched_index].location_id;
      *matched_registry_id = observations[matched_index].registry_id;
    }
  }
  free(observations);
  CFRelease(services);
  return match;
}

static CFArrayRef create_sink_mapping_array(void) {
  CFMutableArrayRef mappings;
  size_t index;

  mappings = CFArrayCreateMutable(kCFAllocatorDefault,
                                  (CFIndex)muse_on_key_filter_mapping_count,
                                  &kCFTypeArrayCallBacks);
  if (!mappings) return NULL;
  for (index = 0; index < muse_on_key_filter_mapping_count; ++index) {
    const void *keys[] = {CFSTR(kIOHIDKeyboardModifierMappingSrcKey),
                          CFSTR(kIOHIDKeyboardModifierMappingDstKey)};
    const void *values[2];
    uint64_t source = muse_on_key_filter_mappings[index].source;
    uint64_t destination = muse_on_key_filter_mappings[index].destination;
    CFNumberRef source_number =
        CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt64Type, &source);
    CFNumberRef destination_number =
        CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt64Type, &destination);
    CFDictionaryRef mapping;

    if (!source_number || !destination_number) {
      if (source_number) CFRelease(source_number);
      if (destination_number) CFRelease(destination_number);
      CFRelease(mappings);
      return NULL;
    }
    values[0] = source_number;
    values[1] = destination_number;
    mapping = CFDictionaryCreate(kCFAllocatorDefault, keys, values, 2,
                                 &kCFTypeDictionaryKeyCallBacks,
                                 &kCFTypeDictionaryValueCallBacks);
    CFRelease(source_number);
    CFRelease(destination_number);
    if (!mapping) {
      CFRelease(mappings);
      return NULL;
    }
    CFArrayAppendValue(mappings, mapping);
    CFRelease(mapping);
  }
  return mappings;
}

static bool service_property_equals(IOHIDServiceClientRef service,
                                    CFTypeRef expected) {
  CFTypeRef readback;
  bool equal;

  if (!service || !expected) return false;
  readback = IOHIDServiceClientCopyProperty(service,
                                            CFSTR(kIOHIDUserKeyUsageMapKey));
  equal = readback && CFEqual(readback, expected);
  if (readback) CFRelease(readback);
  return equal;
}

static bool restore_service_mapping(IOHIDServiceClientRef service,
                                    CFTypeRef original_mapping,
                                    bool original_mapping_was_null) {
  CFArrayRef empty_mapping = NULL;
  CFTypeRef desired = original_mapping;
  CFTypeRef readback;
  bool verified = false;

  if (!service) return false;
  if (original_mapping_was_null) {
    empty_mapping = CFArrayCreate(kCFAllocatorDefault, NULL, 0,
                                  &kCFTypeArrayCallBacks);
    if (!empty_mapping) return false;
    desired = empty_mapping;
  }
  if (!desired || !IOHIDServiceClientSetProperty(
                       service, CFSTR(kIOHIDUserKeyUsageMapKey), desired)) {
    if (empty_mapping) CFRelease(empty_mapping);
    return false;
  }
  readback = IOHIDServiceClientCopyProperty(service,
                                            CFSTR(kIOHIDUserKeyUsageMapKey));
  if (original_mapping_was_null) {
    bool readback_is_null = readback == NULL;
    size_t readback_count = 1;
    if (readback && CFGetTypeID(readback) == CFArrayGetTypeID()) {
      readback_count = (size_t)CFArrayGetCount((CFArrayRef)readback);
    }
    verified = muse_on_key_filter_restore_is_verified(
        true, readback_is_null, readback_count);
  } else {
    verified = readback && CFEqual(readback, original_mapping);
  }
  if (readback) CFRelease(readback);
  if (empty_mapping) CFRelease(empty_mapping);
  return verified;
}

static void clear_saved_mapping(MuseOnKeyFilter *filter) {
  if (filter->original_mapping) CFRelease(filter->original_mapping);
  filter->original_mapping = NULL;
  filter->original_mapping_was_null = false;
  filter->active_location_id = 0;
  filter->active_registry_id = 0;
  filter->restore_pending = false;
  filter->active = false;
}

MuseOnKeyFilter *muse_on_key_filter_create(void) {
  MuseOnKeyFilter *filter = calloc(1, sizeof(*filter));

  if (!filter) return NULL;
  filter->client = IOHIDEventSystemClientCreateSimpleClient(kCFAllocatorDefault);
  if (!filter->client) {
    free(filter);
    return NULL;
  }
  return filter;
}

void muse_on_key_filter_destroy(MuseOnKeyFilter *filter) {
  if (!filter) return;
  (void)muse_on_key_filter_restore(filter);
  if (filter->original_mapping) CFRelease(filter->original_mapping);
  if (filter->client) CFRelease(filter->client);
  free(filter);
}

bool muse_on_key_filter_apply(MuseOnKeyFilter *filter,
                              uint64_t requested_location_id,
                              uint64_t requested_registry_id) {
  IOHIDServiceClientRef service;
  CFTypeRef original_mapping;
  CFArrayRef sink_mapping;
  uint64_t matched_location_id;
  uint64_t matched_registry_id;
  MuseOnKeyFilterLookupResult lookup_result;
  bool property_set;
  bool verified;

  if (!filter || filter->active || filter->restore_pending ||
      !filter->client || requested_location_id == 0 ||
      requested_registry_id == 0 ||
      !muse_on_key_filter_mapping_table_is_valid()) {
    return false;
  }
  service = find_keyboard_service(
      filter->client, requested_location_id, requested_registry_id,
      &matched_location_id, &matched_registry_id, &lookup_result);
  if (!service) return false;
  original_mapping = IOHIDServiceClientCopyProperty(
      service, CFSTR(kIOHIDUserKeyUsageMapKey));
  if (original_mapping && CFGetTypeID(original_mapping) != CFArrayGetTypeID()) {
    CFRelease(original_mapping);
    CFRelease(service);
    return false;
  }
  sink_mapping = create_sink_mapping_array();
  if (!sink_mapping) {
    if (original_mapping) CFRelease(original_mapping);
    CFRelease(service);
    return false;
  }
  /*
   * A prior force-quit can leave this exact temporary sink table behind.
   * Treat only our byte-for-byte table as stale session state, so a normal
   * relaunch can restore the device to an empty mapping on exit.
   */
  if (original_mapping && CFEqual(original_mapping, sink_mapping)) {
    CFRelease(original_mapping);
    original_mapping = NULL;
  }

  /*
   * Save the exact pre-existing value before attempting the write. Even if
   * SetProperty or readback reports failure, the kernel state can be uncertain;
   * keeping restore_pending makes all callers fail closed while retaining a
   * recovery path.
   */
  filter->original_mapping = original_mapping;
  filter->original_mapping_was_null = original_mapping == NULL;
  filter->active_location_id = matched_location_id;
  filter->active_registry_id = matched_registry_id;
  filter->restore_pending = true;

  property_set = IOHIDServiceClientSetProperty(
      service, CFSTR(kIOHIDUserKeyUsageMapKey), sink_mapping);
  verified = property_set && service_property_equals(service, sink_mapping);
  CFRelease(sink_mapping);
  CFRelease(service);
  if (!verified) {
    (void)muse_on_key_filter_restore(filter);
    return false;
  }
  filter->active = true;
  return true;
}

bool muse_on_key_filter_restore(MuseOnKeyFilter *filter) {
  IOHIDServiceClientRef service;
  uint64_t matched_location_id;
  uint64_t matched_registry_id;
  MuseOnKeyFilterLookupResult lookup_result;
  bool restored;

  if (!filter || !filter->client) return false;
  filter->active = false;
  if (!filter->restore_pending) return true;
  service = find_keyboard_service(
      filter->client, filter->active_location_id, filter->active_registry_id,
      &matched_location_id, &matched_registry_id, &lookup_result);
  if (!service) {
    if (lookup_result != MUSE_ON_KEY_FILTER_LOOKUP_CONFIRMED_ABSENT) {
      return false;
    }
    /* A per-device UserKeyMapping disappears with the disconnected service. */
    clear_saved_mapping(filter);
    return true;
  }
  restored = restore_service_mapping(service, filter->original_mapping,
                                     filter->original_mapping_was_null);
  CFRelease(service);
  if (!restored) return false;
  clear_saved_mapping(filter);
  return true;
}

bool muse_on_key_filter_is_active(const MuseOnKeyFilter *filter) {
  return filter && filter->active;
}

bool muse_on_key_filter_needs_restore(const MuseOnKeyFilter *filter) {
  return filter && filter->restore_pending;
}

uint64_t muse_on_key_filter_location_id(const MuseOnKeyFilter *filter) {
  return filter ? filter->active_location_id : 0;
}

uint64_t muse_on_key_filter_registry_id(const MuseOnKeyFilter *filter) {
  return filter ? filter->active_registry_id : 0;
}
