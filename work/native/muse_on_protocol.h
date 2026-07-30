#ifndef MUSE_ON_PROTOCOL_H
#define MUSE_ON_PROTOCOL_H

#import <Foundation/Foundation.h>

#include <math.h>
#include <stdint.h>

#include "muse_on_state_coordinator.h"

static inline BOOL muse_on_protocol_read_json_string(
    NSDictionary *event, NSString *key, NSString **value) {
  id candidate;

  if (![event isKindOfClass:[NSDictionary class]] ||
      ![key isKindOfClass:[NSString class]] || !value) {
    return NO;
  }
  candidate = event[key];
  if (![candidate isKindOfClass:[NSString class]]) return NO;
  *value = candidate;
  return YES;
}

static inline BOOL muse_on_protocol_read_recovery_tuple(
    NSDictionary *event, MuseOnRecoveryOutcome *outcome,
    MuseOnSafetyFailure *failure) {
  NSString *outcomeName;
  NSString *failureName;

  if (!outcome || !failure ||
      !muse_on_protocol_read_json_string(
          event, @"recoveryOutcome", &outcomeName) ||
      !muse_on_protocol_read_json_string(
          event, @"recoveryFailure", &failureName)) {
    return NO;
  }
  if ([outcomeName isEqualToString:@"success"] &&
      [failureName isEqualToString:@"none"]) {
    *outcome = MUSE_ON_RECOVERY_OUTCOME_SUCCESS;
    *failure = MUSE_ON_SAFETY_FAILURE_NONE;
    return YES;
  }
  if ([outcomeName isEqualToString:@"neutral_entry_pending"] &&
      [failureName isEqualToString:@"none"]) {
    *outcome = MUSE_ON_RECOVERY_OUTCOME_NEUTRAL_ENTRY_PENDING;
    *failure = MUSE_ON_SAFETY_FAILURE_NONE;
    return YES;
  }
  if ([outcomeName isEqualToString:@"failure"] &&
      [failureName isEqualToString:@"device_state_uncertain"]) {
    *outcome = MUSE_ON_RECOVERY_OUTCOME_FAILURE;
    *failure = MUSE_ON_SAFETY_FAILURE_DEVICE_UNCERTAIN;
    return YES;
  }
  return NO;
}

static inline BOOL muse_on_protocol_read_json_boolean(
    NSDictionary *event, NSString *key, BOOL *value) {
  id candidate;

  if (![event isKindOfClass:[NSDictionary class]] ||
      ![key isKindOfClass:[NSString class]] || !value) {
    return NO;
  }
  candidate = event[key];
  if (!candidate ||
      CFGetTypeID((__bridge CFTypeRef)candidate) != CFBooleanGetTypeID()) {
    return NO;
  }
  *value = CFBooleanGetValue((__bridge CFBooleanRef)candidate);
  return YES;
}

static inline BOOL muse_on_protocol_read_json_uint32(
    NSDictionary *event, NSString *key, uint32_t *value) {
  id candidate;
  CFTypeRef candidateRef;
  double numericValue;
  int64_t integralValue;

  if (![event isKindOfClass:[NSDictionary class]] ||
      ![key isKindOfClass:[NSString class]] || !value) {
    return NO;
  }
  candidate = event[key];
  if (!candidate) return NO;
  candidateRef = (__bridge CFTypeRef)candidate;
  if (CFGetTypeID(candidateRef) == CFBooleanGetTypeID() ||
      CFGetTypeID(candidateRef) != CFNumberGetTypeID()) {
    return NO;
  }
  if (!CFNumberGetValue((CFNumberRef)candidateRef, kCFNumberDoubleType,
                        &numericValue) ||
      !isfinite(numericValue) || trunc(numericValue) != numericValue ||
      numericValue < 0 || numericValue > UINT32_MAX ||
      !CFNumberGetValue((CFNumberRef)candidateRef, kCFNumberSInt64Type,
                        &integralValue) ||
      (double)integralValue != numericValue) {
    return NO;
  }
  *value = (uint32_t)integralValue;
  return YES;
}

#endif
