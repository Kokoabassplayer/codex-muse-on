#include "muse_on_key_filter.h"

#include <stdlib.h>

#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOKitLib.h>
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
  bool original_mapping_was_null;
  MuseOnKeyFilterOwnership ownership;
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

MuseOnKeyFilterRemovalDecision muse_on_key_filter_removal_decide(
    bool keyboard_removed, const MuseOnKeyFilterOwnership *ownership,
    uint64_t removed_location_id, uint64_t removed_registry_id,
    bool controller_topology_valid, bool filter_applied) {
  if (keyboard_removed && ownership && ownership->restore_pending &&
      ownership->location_id != 0 && ownership->registry_id != 0 &&
      ownership->location_id == removed_location_id &&
      ownership->registry_id == removed_registry_id) {
    return controller_topology_valid
               ? MUSE_ON_KEY_FILTER_REMOVAL_CONFIRM_EXACT_TARGET_AND_REAPPLY
               : MUSE_ON_KEY_FILTER_REMOVAL_CONFIRM_EXACT_TARGET;
  }
  if (filter_applied && !controller_topology_valid) {
    return MUSE_ON_KEY_FILTER_REMOVAL_RESTORE_GENERIC;
  }
  if (keyboard_removed && ownership && ownership->restore_pending &&
      ownership->location_id != 0 &&
      ownership->location_id == removed_location_id) {
    return MUSE_ON_KEY_FILTER_REMOVAL_RESTORE_GENERIC;
  }
  return MUSE_ON_KEY_FILTER_REMOVAL_NONE;
}

bool muse_on_key_filter_ownership_confirm_removed(
    MuseOnKeyFilterOwnership *ownership, uint64_t removed_location_id,
    uint64_t removed_registry_id) {
  MuseOnKeyFilterRemovalDecision decision =
      muse_on_key_filter_removal_decide(true, ownership, removed_location_id,
                                       removed_registry_id, false, false);
  if (decision != MUSE_ON_KEY_FILTER_REMOVAL_CONFIRM_EXACT_TARGET) {
    return false;
  }
  muse_on_key_filter_ownership_clear(ownership);
  return true;
}

void muse_on_key_filter_ownership_clear(MuseOnKeyFilterOwnership *ownership) {
  if (ownership) *ownership = (MuseOnKeyFilterOwnership){0};
}

bool muse_on_key_filter_removal_may_reapply(
    MuseOnKeyFilterRemovalDecision decision, bool hold_release_succeeded,
    bool release_failed) {
  return decision ==
             MUSE_ON_KEY_FILTER_REMOVAL_CONFIRM_EXACT_TARGET_AND_REAPPLY &&
         hold_release_succeeded && !release_failed;
}

bool muse_on_key_filter_reapply_failed_terminally(
    bool filter_applied, bool apply_settling, bool safety_failure_present) {
  return !filter_applied && !apply_settling && !safety_failure_present;
}

bool muse_on_key_filter_registry_chain_contains(
    const uint64_t *registry_ids, size_t registry_id_count,
    uint64_t requested_registry_id) {
  size_t index;

  if ((!registry_ids && registry_id_count != 0) ||
      requested_registry_id == 0) {
    return false;
  }
  for (index = 0; index < registry_id_count; ++index) {
    if (registry_ids[index] == requested_registry_id) return true;
  }
  return false;
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

void muse_on_key_filter_apply_policy_init(MuseOnKeyFilterApplyPolicy *policy) {
  if (!policy) return;
  policy->waiting = false;
  policy->started_at_ns = 0;
}

MuseOnKeyFilterApplyDecision muse_on_key_filter_apply_policy_evaluate(
    MuseOnKeyFilterApplyPolicy *policy, uint64_t now_ns,
    MuseOnKeyFilterApplyResult result) {
  if (!policy) return MUSE_ON_KEY_FILTER_APPLY_FAILED;
  if (result == MUSE_ON_KEY_FILTER_APPLY_APPLIED) {
    muse_on_key_filter_apply_policy_init(policy);
    return MUSE_ON_KEY_FILTER_APPLY_COMPLETE;
  }
  if (result == MUSE_ON_KEY_FILTER_APPLY_ROLLBACK_UNVERIFIED ||
      result == MUSE_ON_KEY_FILTER_APPLY_FATAL) {
    muse_on_key_filter_apply_policy_init(policy);
    return MUSE_ON_KEY_FILTER_APPLY_FAILED;
  }
  if (!policy->waiting) {
    policy->waiting = true;
    policy->started_at_ns = now_ns;
    return MUSE_ON_KEY_FILTER_APPLY_RETRY;
  }
  if (now_ns < policy->started_at_ns ||
      now_ns - policy->started_at_ns >=
          MUSE_ON_KEY_FILTER_APPLY_SETTLEMENT_NS) {
    muse_on_key_filter_apply_policy_init(policy);
    return MUSE_ON_KEY_FILTER_APPLY_FAILED;
  }
  return MUSE_ON_KEY_FILTER_APPLY_RETRY;
}

const char *muse_on_key_filter_apply_result_string(
    MuseOnKeyFilterApplyResult result) {
  switch (result) {
    case MUSE_ON_KEY_FILTER_APPLY_APPLIED:
      return "apply_keyboard_filter_applied";
    case MUSE_ON_KEY_FILTER_APPLY_SERVICE_ABSENT:
      return "apply_keyboard_filter_service_absent";
    case MUSE_ON_KEY_FILTER_APPLY_SERVICE_UNCERTAIN:
      return "apply_keyboard_filter_service_uncertain";
    case MUSE_ON_KEY_FILTER_APPLY_CLIENT_REFRESH_FAILED:
      return "apply_keyboard_filter_client_refresh_failed";
    case MUSE_ON_KEY_FILTER_APPLY_WRITE_FAILED:
      return "apply_keyboard_filter_write_failed";
    case MUSE_ON_KEY_FILTER_APPLY_VERIFY_FAILED:
      return "apply_keyboard_filter_verify_failed";
    case MUSE_ON_KEY_FILTER_APPLY_ROLLBACK_UNVERIFIED:
      return "apply_keyboard_filter_rollback_unverified";
    case MUSE_ON_KEY_FILTER_APPLY_FATAL:
      return "apply_keyboard_filter_fatal";
  }
  return "apply_keyboard_filter_unknown";
}

bool muse_on_key_filter_refresh_and_find_service(
    const MuseOnKeyFilterClientOperations *operations, void *context,
    void *existing_client, uint64_t requested_location_id,
    uint64_t requested_registry_id, void **refreshed_client,
    void **matched_service, MuseOnKeyFilterLookupResult *lookup_result) {
  void *client;
  void *service;

  if (refreshed_client) *refreshed_client = NULL;
  if (matched_service) *matched_service = NULL;
  if (!operations || !operations->create_client ||
      !operations->release_client || !operations->find_service ||
      !refreshed_client || !matched_service || !lookup_result ||
      requested_location_id == 0 || requested_registry_id == 0) {
    return false;
  }
  client = operations->create_client(context);
  if (!client) return false;
  service = operations->find_service(
      context, client, requested_location_id, requested_registry_id,
      lookup_result);
  if (existing_client) operations->release_client(context, existing_client);
  *refreshed_client = client;
  *matched_service = service;
  return true;
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

static bool copy_registry_ancestor_id(IOHIDServiceClientRef service,
                                      uint64_t requested_registry_id,
                                      uint64_t *value) {
  CFTypeRef registry_id;
  CFMutableDictionaryRef matching;
  io_registry_entry_t entry;
  uint64_t registry_chain[64];
  size_t registry_chain_count = 0;
  int64_t signed_value;
  bool complete = false;

  if (!service || requested_registry_id == 0 || !value) return false;
  *value = 0;
  registry_id = IOHIDServiceClientGetRegistryID(service);
  if (!registry_id || CFGetTypeID(registry_id) != CFNumberGetTypeID() ||
      !CFNumberGetValue((CFNumberRef)registry_id, kCFNumberSInt64Type,
                        &signed_value) ||
      signed_value <= 0) {
    return false;
  }
  matching = IORegistryEntryIDMatching((uint64_t)signed_value);
  if (!matching) return false;
  entry = IOServiceGetMatchingService(kIOMainPortDefault, matching);
  if (entry == IO_OBJECT_NULL) return false;

  while (entry != IO_OBJECT_NULL &&
         registry_chain_count <
             sizeof(registry_chain) / sizeof(registry_chain[0])) {
    io_registry_entry_t parent = IO_OBJECT_NULL;
    uint64_t current_id = 0;
    kern_return_t result;

    result = IORegistryEntryGetRegistryEntryID(entry, &current_id);
    if (result != KERN_SUCCESS || current_id == 0) {
      IOObjectRelease(entry);
      return false;
    }
    registry_chain[registry_chain_count++] = current_id;
    if (current_id == requested_registry_id) {
      complete = true;
      IOObjectRelease(entry);
      break;
    }

    result = IORegistryEntryGetParentEntry(entry, kIOServicePlane, &parent);
    IOObjectRelease(entry);
    entry = IO_OBJECT_NULL;
    if (result == KERN_SUCCESS && parent != IO_OBJECT_NULL) {
      entry = parent;
      continue;
    }
    if (result == kIOReturnNoDevice || result == kIOReturnNotFound) {
      complete = true;
    }
    break;
  }
  if (entry != IO_OBJECT_NULL) IOObjectRelease(entry);
  if (!complete) return false;
  if (muse_on_key_filter_registry_chain_contains(
          registry_chain, registry_chain_count, requested_registry_id)) {
    *value = requested_registry_id;
  }
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
    observation->registry_id_readable = copy_registry_ancestor_id(
        service, requested_registry_id, &registry_id);
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

static void *create_event_system_client(void *context) {
  (void)context;
  return IOHIDEventSystemClientCreateSimpleClient(kCFAllocatorDefault);
}

static void release_event_system_client(void *context, void *client) {
  (void)context;
  if (client) CFRelease((CFTypeRef)client);
}

static void *find_event_system_keyboard_service(
    void *context, void *client, uint64_t requested_location_id,
    uint64_t requested_registry_id, MuseOnKeyFilterLookupResult *lookup_result) {
  uint64_t matched_location_id = 0;
  uint64_t matched_registry_id = 0;

  (void)context;
  return find_keyboard_service(
      (IOHIDEventSystemClientRef)client, requested_location_id,
      requested_registry_id, &matched_location_id, &matched_registry_id,
      lookup_result);
}

static const MuseOnKeyFilterClientOperations key_filter_client_operations = {
    .create_client = create_event_system_client,
    .release_client = release_event_system_client,
    .find_service = find_event_system_keyboard_service,
};

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
  muse_on_key_filter_ownership_clear(&filter->ownership);
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

MuseOnKeyFilterApplyResult muse_on_key_filter_apply(
    MuseOnKeyFilter *filter, uint64_t requested_location_id,
    uint64_t requested_registry_id) {
  IOHIDServiceClientRef service;
  void *refreshed_client = NULL;
  void *matched_service = NULL;
  CFTypeRef original_mapping;
  CFArrayRef sink_mapping;
  MuseOnKeyFilterLookupResult lookup_result;
  bool property_set;
  bool verified;
  bool rollback_verified;

  if (!filter || filter->ownership.active ||
      filter->ownership.restore_pending || requested_location_id == 0 ||
      requested_registry_id == 0 ||
      !muse_on_key_filter_mapping_table_is_valid()) {
    return MUSE_ON_KEY_FILTER_APPLY_FATAL;
  }
  if (!muse_on_key_filter_refresh_and_find_service(
          &key_filter_client_operations, NULL, filter->client,
          requested_location_id, requested_registry_id, &refreshed_client,
          &matched_service, &lookup_result)) {
    return MUSE_ON_KEY_FILTER_APPLY_CLIENT_REFRESH_FAILED;
  }
  filter->client = (IOHIDEventSystemClientRef)refreshed_client;
  service = (IOHIDServiceClientRef)matched_service;
  if (!service) {
    return lookup_result == MUSE_ON_KEY_FILTER_LOOKUP_CONFIRMED_ABSENT
               ? MUSE_ON_KEY_FILTER_APPLY_SERVICE_ABSENT
               : MUSE_ON_KEY_FILTER_APPLY_SERVICE_UNCERTAIN;
  }
  original_mapping = IOHIDServiceClientCopyProperty(
      service, CFSTR(kIOHIDUserKeyUsageMapKey));
  if (original_mapping && CFGetTypeID(original_mapping) != CFArrayGetTypeID()) {
    CFRelease(original_mapping);
    CFRelease(service);
    return MUSE_ON_KEY_FILTER_APPLY_FATAL;
  }
  sink_mapping = create_sink_mapping_array();
  if (!sink_mapping) {
    if (original_mapping) CFRelease(original_mapping);
    CFRelease(service);
    return MUSE_ON_KEY_FILTER_APPLY_FATAL;
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
  filter->ownership.location_id = requested_location_id;
  filter->ownership.registry_id = requested_registry_id;
  filter->ownership.restore_pending = true;

  property_set = IOHIDServiceClientSetProperty(
      service, CFSTR(kIOHIDUserKeyUsageMapKey), sink_mapping);
  verified = property_set && service_property_equals(service, sink_mapping);
  CFRelease(sink_mapping);
  CFRelease(service);
  if (!verified) {
    rollback_verified = muse_on_key_filter_restore(filter);
    if (!rollback_verified) {
      return MUSE_ON_KEY_FILTER_APPLY_ROLLBACK_UNVERIFIED;
    }
    return property_set ? MUSE_ON_KEY_FILTER_APPLY_VERIFY_FAILED
                        : MUSE_ON_KEY_FILTER_APPLY_WRITE_FAILED;
  }
  filter->ownership.active = true;
  return MUSE_ON_KEY_FILTER_APPLY_APPLIED;
}

bool muse_on_key_filter_restore(MuseOnKeyFilter *filter) {
  IOHIDServiceClientRef service;
  uint64_t matched_location_id;
  uint64_t matched_registry_id;
  MuseOnKeyFilterLookupResult lookup_result;
  bool restored;

  if (!filter || !filter->client) return false;
  filter->ownership.active = false;
  if (!filter->ownership.restore_pending) return true;
  service = find_keyboard_service(
      filter->client, filter->ownership.location_id,
      filter->ownership.registry_id,
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

bool muse_on_key_filter_confirm_device_removed(
    MuseOnKeyFilter *filter, uint64_t removed_location_id,
    uint64_t removed_registry_id) {
  if (!filter || !muse_on_key_filter_ownership_confirm_removed(
                     &filter->ownership, removed_location_id,
                     removed_registry_id)) {
    return false;
  }
  /* UserKeyMapping belongs to this exact per-device service. Its confirmed
   * removal destroys that mapping; a replacement at the same USB location is
   * a different registry identity and must never be used as a restore target. */
  clear_saved_mapping(filter);
  return true;
}

bool muse_on_key_filter_is_active(const MuseOnKeyFilter *filter) {
  return filter && filter->ownership.active;
}

bool muse_on_key_filter_needs_restore(const MuseOnKeyFilter *filter) {
  return filter && filter->ownership.restore_pending;
}

uint64_t muse_on_key_filter_location_id(const MuseOnKeyFilter *filter) {
  return filter ? filter->ownership.location_id : 0;
}

uint64_t muse_on_key_filter_registry_id(const MuseOnKeyFilter *filter) {
  return filter ? filter->ownership.registry_id : 0;
}
