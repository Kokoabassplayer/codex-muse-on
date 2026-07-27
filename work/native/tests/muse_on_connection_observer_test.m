#import <Foundation/Foundation.h>

#include <assert.h>
#include <CoreFoundation/CoreFoundation.h>
#include <dispatch/dispatch.h>
#include <IOKit/hid/IOHIDUsageTables.h>

#include "../muse_on_connection_observer.h"

typedef struct {
  uint32_t vendor_id;
  uint32_t product_id;
  uint32_t usage_page;
  uint32_t usage;
  uint32_t location_id;
} MuseOnConnectionObserverTestDevice;

extern MuseOnConnectionSnapshot muse_on_connection_observer_test_snapshot(
    const MuseOnConnectionObserverTestDevice *devices, size_t count);

enum {
  kTestMuseOnVendorID = 0x04b4,
  kTestMuseOnProductID = 0xe106,
};

typedef struct {
  bool started;
  bool stopped;
  size_t start_count;
  size_t stop_count;
  MuseOnConnectionObserverEmit emit;
  void *emit_context;
  size_t dispatch_count;
  const MuseOnConnectionObserverTestDevice *devices;
  size_t device_count;
} FakeObserverBackend;

static MuseOnConnectionObserverTestDevice fake_device(uint32_t usage,
                                                       uint32_t location) {
  return (MuseOnConnectionObserverTestDevice){
      kTestMuseOnVendorID, kTestMuseOnProductID, kHIDPage_GenericDesktop, usage,
      location};
}

static bool fake_start(void *context, MuseOnConnectionObserverEmit emit,
                       void *emit_context) {
  FakeObserverBackend *fake = context;
  fake->started = true;
  fake->stopped = false;
  fake->start_count++;
  fake->emit = emit;
  fake->emit_context = emit_context;
  emit(emit_context, muse_on_connection_observer_test_snapshot(
                         fake->devices, fake->device_count));
  return true;
}

static void fake_stop(void *context) {
  FakeObserverBackend *fake = context;
  fake->started = false;
  fake->stopped = true;
  fake->stop_count++;
}

static void fake_emit(FakeObserverBackend *fake,
                      MuseOnConnectionSnapshot snapshot) {
  assert(fake->emit != NULL);
  fake->emit(fake->emit_context, snapshot);
}

typedef struct {
  size_t callback_count;
  MuseOnConnectionSnapshot last_snapshot;
  bool callback_was_on_main;
} CallbackState;

static void record_snapshot(void *context,
                            MuseOnConnectionSnapshot snapshot) {
  CallbackState *state = context;
  state->callback_count++;
  state->last_snapshot = snapshot;
  state->callback_was_on_main = [NSThread isMainThread];
}

static void drain_main_queue(void) {
  dispatch_async(dispatch_get_main_queue(), ^{
    CFRunLoopStop(CFRunLoopGetMain());
  });
  CFRunLoopRun();
}

static void test_fake_backend_ignores_mouse_and_preserves_topology(void) {
  MuseOnConnectionObserverTestDevice complete_with_mouse[] = {
      fake_device(kHIDUsage_GD_Keyboard, 0x110000),
      fake_device(kHIDUsage_GD_Joystick, 0x110000),
      fake_device(kHIDUsage_GD_Mouse, 0x110000),
  };
  MuseOnConnectionObserverTestDevice partial_with_mouse[] = {
      fake_device(kHIDUsage_GD_Keyboard, 0x110000),
      fake_device(kHIDUsage_GD_Mouse, 0x110000),
  };
  MuseOnConnectionObserverTestDevice mismatched_with_mouse[] = {
      fake_device(kHIDUsage_GD_Keyboard, 1),
      fake_device(kHIDUsage_GD_Joystick, 2),
      fake_device(kHIDUsage_GD_Mouse, 1),
  };
  MuseOnConnectionObserverTestDevice duplicate_with_mouse[] = {
      fake_device(kHIDUsage_GD_Keyboard, 1),
      fake_device(kHIDUsage_GD_Keyboard, 1),
      fake_device(kHIDUsage_GD_Joystick, 1),
      fake_device(kHIDUsage_GD_Mouse, 1),
  };
  MuseOnConnectionObserverTestDevice multiple_with_mouse[] = {
      fake_device(kHIDUsage_GD_Keyboard, 1),
      fake_device(kHIDUsage_GD_Joystick, 1),
      fake_device(kHIDUsage_GD_Keyboard, 2),
      fake_device(kHIDUsage_GD_Joystick, 2),
      fake_device(kHIDUsage_GD_Mouse, 1),
  };

  assert(muse_on_connection_observer_test_snapshot(
             complete_with_mouse, sizeof(complete_with_mouse) /
                                      sizeof(complete_with_mouse[0]))
             .state == MUSE_ON_CONNECTION_SINGLE);
  assert(muse_on_connection_observer_test_snapshot(
             partial_with_mouse,
             sizeof(partial_with_mouse) / sizeof(partial_with_mouse[0]))
             .state == MUSE_ON_CONNECTION_DISCONNECTED);
  assert(muse_on_connection_observer_test_snapshot(
             mismatched_with_mouse,
             sizeof(mismatched_with_mouse) / sizeof(mismatched_with_mouse[0]))
             .state == MUSE_ON_CONNECTION_DISCONNECTED);
  assert(muse_on_connection_observer_test_snapshot(
             duplicate_with_mouse,
             sizeof(duplicate_with_mouse) / sizeof(duplicate_with_mouse[0]))
             .state == MUSE_ON_CONNECTION_DISCONNECTED);
  assert(muse_on_connection_observer_test_snapshot(
             multiple_with_mouse,
             sizeof(multiple_with_mouse) / sizeof(multiple_with_mouse[0]))
             .state == MUSE_ON_CONNECTION_MULTIPLE);
}

static void test_injected_observer_is_main_queue_and_fail_closed_on_stop(void) {
  FakeObserverBackend fake = {0};
  CallbackState callback = {0};
  MuseOnConnectionObserverTestDevice devices[] = {
      fake_device(kHIDUsage_GD_Keyboard, 0x110000),
      fake_device(kHIDUsage_GD_Joystick, 0x110000),
      fake_device(kHIDUsage_GD_Mouse, 0x110000),
  };
  fake.devices = devices;
  fake.device_count = sizeof(devices) / sizeof(devices[0]);
  MuseOnConnectionObserverBackend backend = {
      &fake, fake_start, fake_stop, NULL};
  MuseOnConnectionObserver *observer = [[MuseOnConnectionObserver alloc]
      initWithBackend:backend callback:record_snapshot callbackContext:&callback];

  assert(observer != nil);
  assert([observer start]);
  assert([observer start]);
  assert(fake.start_count == 1);
  drain_main_queue();
  assert(callback.callback_count == 1);
  assert(callback.last_snapshot.state == MUSE_ON_CONNECTION_SINGLE);
  assert(callback.callback_was_on_main);

  fake_emit(&fake,
            (MuseOnConnectionSnapshot){MUSE_ON_CONNECTION_DISCONNECTED, 0});
  drain_main_queue();
  assert(callback.callback_count == 2);
  assert(callback.last_snapshot.state == MUSE_ON_CONNECTION_DISCONNECTED);
  assert(fake.dispatch_count == 0);

  fake_emit(&fake,
            (MuseOnConnectionSnapshot){MUSE_ON_CONNECTION_SINGLE, 0x110000});
  [observer stop];
  assert(fake.stop_count == 1);
  drain_main_queue();
  assert(callback.callback_count == 2);

  fake_emit(&fake,
            (MuseOnConnectionSnapshot){MUSE_ON_CONNECTION_MULTIPLE, 0});
  drain_main_queue();
  assert(callback.callback_count == 2);
}

int main(void) {
  @autoreleasepool {
    test_fake_backend_ignores_mouse_and_preserves_topology();
    test_injected_observer_is_main_queue_and_fail_closed_on_stop();
  }
  return 0;
}
