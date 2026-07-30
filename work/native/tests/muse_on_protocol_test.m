#import <Foundation/Foundation.h>

#include <assert.h>

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
  }
  return 0;
}
