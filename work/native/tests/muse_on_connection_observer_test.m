#import <Foundation/Foundation.h>

#include <assert.h>
#include <CoreFoundation/CoreFoundation.h>
#include <dispatch/dispatch.h>

#include "../muse_on_connection_observer.h"

typedef struct {
  bool started;
  bool stopped;
  size_t start_count;
  size_t stop_count;
  MuseOnConnectionObserverEmit emit;
  void *emit_context;
  size_t dispatch_count;
} FakeObserverBackend;

static bool fake_start(void *context, MuseOnConnectionObserverEmit emit,
                       void *emit_context) {
  FakeObserverBackend *fake = context;
  fake->started = true;
  fake->stopped = false;
  fake->start_count++;
  fake->emit = emit;
  fake->emit_context = emit_context;
  emit(emit_context,
       (MuseOnConnectionSnapshot){MUSE_ON_CONNECTION_SINGLE, 0x110000});
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

static void test_injected_observer_is_main_queue_and_fail_closed_on_stop(void) {
  FakeObserverBackend fake = {0};
  CallbackState callback = {0};
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
    test_injected_observer_is_main_queue_and_fail_closed_on_stop();
  }
  return 0;
}
