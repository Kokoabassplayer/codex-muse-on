#ifndef MUSE_ON_PROTOCOL_H
#define MUSE_ON_PROTOCOL_H

#import <Foundation/Foundation.h>

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

#endif
