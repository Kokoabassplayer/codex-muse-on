#import <Foundation/Foundation.h>

#include <assert.h>
#include <math.h>
#include <stdint.h>

#import "../muse_on_protocol.h"

static NSDictionary *json_object(NSString *literal) {
  NSData *data = [literal dataUsingEncoding:NSUTF8StringEncoding];
  NSError *error = nil;
  id object = [NSJSONSerialization JSONObjectWithData:data options:0
                                                error:&error];

  assert(error == nil);
  assert([object isKindOfClass:[NSDictionary class]]);
  return object;
}

static void assert_boolean(NSString *literal, BOOL valid, BOOL expected) {
  BOOL value = !expected;
  BOOL result = muse_on_protocol_read_json_boolean(
      json_object(literal), @"proof", &value);

  assert(result == valid);
  if (valid) assert(value == expected);
}

static void assert_string(NSString *literal, BOOL valid, NSString *expected) {
  NSString *value = nil;
  BOOL result = muse_on_protocol_read_json_string(
      json_object(literal), @"proof", &value);

  assert(result == valid);
  if (valid) assert([value isEqualToString:expected]);
}

static void assert_uint32(NSString *literal, BOOL valid, uint32_t expected) {
  uint32_t value = expected == 0 ? UINT32_MAX : 0;
  BOOL result = muse_on_protocol_read_json_uint32(
      json_object(literal), @"proof", &value);

  assert(result == valid);
  if (valid) assert(value == expected);
}

int main(void) {
  @autoreleasepool {
    assert_boolean(@"{\"proof\":true}", YES, YES);
    assert_boolean(@"{\"proof\":false}", YES, NO);
    assert_boolean(@"{\"proof\":1}", NO, NO);
    assert_boolean(@"{\"proof\":0}", NO, NO);
    assert_boolean(@"{\"proof\":\"true\"}", NO, NO);
    assert_boolean(@"{\"proof\":null}", NO, NO);
    assert_boolean(@"{}", NO, NO);
    assert_boolean(@"{\"proof\":[]}", NO, NO);
    assert_boolean(@"{\"proof\":{}}", NO, NO);

    assert_string(@"{\"proof\":\"success\"}", YES, @"success");
    assert_string(@"{\"proof\":\"\"}", YES, @"");
    assert_string(@"{\"proof\":1}", NO, nil);
    assert_string(@"{\"proof\":true}", NO, nil);
    assert_string(@"{\"proof\":null}", NO, nil);
    assert_string(@"{}", NO, nil);
    assert_string(@"{\"proof\":[]}", NO, nil);
    assert_string(@"{\"proof\":{}}", NO, nil);

    assert_uint32(@"{\"proof\":0}", YES, 0);
    assert_uint32(@"{\"proof\":4294967295}", YES, UINT32_MAX);
    assert_uint32(@"{\"proof\":true}", NO, 0);
    assert_uint32(@"{\"proof\":false}", NO, 0);
    assert_uint32(@"{\"proof\":-1}", NO, 0);
    assert_uint32(@"{\"proof\":4294967296}", NO, 0);
    assert_uint32(@"{\"proof\":1.5}", NO, 0);
    assert_uint32(@"{\"proof\":\"1\"}", NO, 0);
    assert_uint32(@"{\"proof\":null}", NO, 0);
    assert_uint32(@"{}", NO, 0);
    assert_uint32(@"{\"proof\":[]}", NO, 0);
    assert_uint32(@"{\"proof\":{}}", NO, 0);
    uint32_t nanValue = 0;
    assert(!muse_on_protocol_read_json_uint32(
        @{@"proof": @(NAN)}, @"proof", &nanValue));
  }
  return 0;
}
