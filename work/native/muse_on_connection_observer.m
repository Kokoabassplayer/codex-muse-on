#import "muse_on_connection_observer.h"

#import <CoreFoundation/CoreFoundation.h>
#import <IOKit/hid/IOHIDManager.h>
#import <IOKit/hid/IOHIDUsageTables.h>

#include <stdlib.h>

enum {
  kMuseOnVendorID = 0x04b4,
  kMuseOnProductID = 0xe106,
  kMuseOnSnapshotCapacity = 64,
};

typedef struct {
  IOHIDManagerRef manager;
  CFMutableArrayRef devices;
  MuseOnConnectionObserverEmit emit;
  void *emit_context;
  bool active;
} MuseOnNativeBackend;

typedef struct {
  uint32_t vendor_id;
  uint32_t product_id;
  uint32_t usage_page;
  uint32_t usage;
  uint32_t location_id;
} MuseOnDeviceIdentity;

static bool read_number(IOHIDDeviceRef device, CFStringRef key,
                        uint32_t *value) {
  CFTypeRef property;

  if (!device || !key || !value) return false;
  property = IOHIDDeviceGetProperty(device, key);
  return property && CFGetTypeID(property) == CFNumberGetTypeID() &&
         CFNumberGetValue((CFNumberRef)property, kCFNumberSInt32Type, value);
}

static bool device_identity(IOHIDDeviceRef device, uint32_t *vendor_id,
                            uint32_t *product_id, uint32_t *usage_page,
                            uint32_t *usage, uint32_t *location_id) {
  return read_number(device, CFSTR(kIOHIDVendorIDKey), vendor_id) &&
         read_number(device, CFSTR(kIOHIDProductIDKey), product_id) &&
         read_number(device, CFSTR(kIOHIDPrimaryUsagePageKey), usage_page) &&
         read_number(device, CFSTR(kIOHIDPrimaryUsageKey), usage) &&
         read_number(device, CFSTR(kIOHIDLocationIDKey), location_id);
}

static MuseOnConnectionSnapshot classify_identities(
    const MuseOnDeviceIdentity *identities, size_t count) {
  MuseOnObservedInterface interfaces[kMuseOnSnapshotCapacity];
  size_t interface_count = 0;
  size_t index;

  if (!identities || count == 0) {
    return (MuseOnConnectionSnapshot){MUSE_ON_CONNECTION_DISCONNECTED, 0};
  }
  for (index = 0; index < count; index++) {
    MuseOnObservedInterfaceKind kind;

    if (identities[index].vendor_id != kMuseOnVendorID ||
        identities[index].product_id != kMuseOnProductID ||
        identities[index].usage_page != kHIDPage_GenericDesktop) {
      return (MuseOnConnectionSnapshot){MUSE_ON_CONNECTION_DISCONNECTED, 0};
    }
    if (identities[index].usage == kHIDUsage_GD_Keyboard) {
      kind = MUSE_ON_OBSERVED_KEYBOARD;
    } else if (identities[index].usage == kHIDUsage_GD_Joystick) {
      kind = MUSE_ON_OBSERVED_JOYSTICK;
    } else {
      continue;
    }
    if (interface_count == kMuseOnSnapshotCapacity) {
      return (MuseOnConnectionSnapshot){MUSE_ON_CONNECTION_DISCONNECTED, 0};
    }
    interfaces[interface_count++] =
        (MuseOnObservedInterface){kind, identities[index].location_id, true};
  }
  return muse_on_classify_connections(interfaces, interface_count);
}

static MuseOnConnectionSnapshot native_snapshot(
    const MuseOnNativeBackend *backend) {
  MuseOnDeviceIdentity identities[kMuseOnSnapshotCapacity];
  CFIndex index;
  CFIndex count;
  size_t identity_count = 0;

  if (!backend || !backend->active || !backend->devices) {
    return (MuseOnConnectionSnapshot){MUSE_ON_CONNECTION_DISCONNECTED, 0};
  }
  count = CFArrayGetCount(backend->devices);
  if (count <= 0) {
    return (MuseOnConnectionSnapshot){MUSE_ON_CONNECTION_DISCONNECTED, 0};
  }
  for (index = 0; index < count; index++) {
    IOHIDDeviceRef device = (IOHIDDeviceRef)CFArrayGetValueAtIndex(
        backend->devices, index);
    MuseOnDeviceIdentity identity = {0};

    if (!device_identity(device, &identity.vendor_id, &identity.product_id,
                         &identity.usage_page, &identity.usage,
                         &identity.location_id)) {
      return (MuseOnConnectionSnapshot){MUSE_ON_CONNECTION_DISCONNECTED, 0};
    }
    if (identity.usage == kHIDUsage_GD_Keyboard ||
        identity.usage == kHIDUsage_GD_Joystick) {
      if (identity_count == kMuseOnSnapshotCapacity) {
        return (MuseOnConnectionSnapshot){MUSE_ON_CONNECTION_DISCONNECTED, 0};
      }
      identities[identity_count++] = identity;
    } else if (identity.vendor_id != kMuseOnVendorID ||
               identity.product_id != kMuseOnProductID ||
               identity.usage_page != kHIDPage_GenericDesktop) {
      return (MuseOnConnectionSnapshot){MUSE_ON_CONNECTION_DISCONNECTED, 0};
    }
  }
  return classify_identities(identities, identity_count);
}

static void emit_snapshot(MuseOnNativeBackend *backend) {
  if (!backend || !backend->active || !backend->emit) return;
  backend->emit(backend->emit_context, native_snapshot(backend));
}

static void emit_disconnected(MuseOnNativeBackend *backend) {
  if (!backend || !backend->active || !backend->emit) return;
  backend->emit(
      backend->emit_context,
      (MuseOnConnectionSnapshot){MUSE_ON_CONNECTION_DISCONNECTED, 0});
}

static void device_added(void *context, IOReturn result, void *sender,
                         IOHIDDeviceRef device) {
  MuseOnNativeBackend *backend = context;
  uint32_t vendor_id = 0;
  uint32_t product_id = 0;
  uint32_t usage_page = 0;
  uint32_t usage = 0;
  uint32_t location_id = 0;

  (void)sender;
  if (!backend || !backend->active || result != kIOReturnSuccess || !device ||
      !device_identity(device, &vendor_id, &product_id, &usage_page, &usage,
                       &location_id)) {
    emit_disconnected(backend);
    return;
  }
  if (vendor_id != kMuseOnVendorID || product_id != kMuseOnProductID ||
      usage_page != kHIDPage_GenericDesktop) {
    emit_disconnected(backend);
    return;
  }
  if (usage != kHIDUsage_GD_Keyboard && usage != kHIDUsage_GD_Joystick) {
    return;
  }
  if (!CFArrayContainsValue(backend->devices,
                            CFRangeMake(0, CFArrayGetCount(backend->devices)),
                            device)) {
    CFArrayAppendValue(backend->devices, device);
  }
  emit_snapshot(backend);
}

static void device_removed(void *context, IOReturn result, void *sender,
                           IOHIDDeviceRef device) {
  MuseOnNativeBackend *backend = context;
  CFIndex index;

  (void)sender;
  if (!backend || !backend->active || result != kIOReturnSuccess || !device) {
    emit_disconnected(backend);
    return;
  }
  index = CFArrayGetFirstIndexOfValue(
      backend->devices, CFRangeMake(0, CFArrayGetCount(backend->devices)),
      device);
  if (index != kCFNotFound) CFArrayRemoveValueAtIndex(backend->devices, index);
  emit_snapshot(backend);
}

static CFDictionaryRef create_match_dictionary(void) {
  int vendor_id = kMuseOnVendorID;
  int product_id = kMuseOnProductID;
  CFNumberRef vendor = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType,
                                      &vendor_id);
  CFNumberRef product = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType,
                                       &product_id);
  const void *keys[] = {CFSTR(kIOHIDVendorIDKey), CFSTR(kIOHIDProductIDKey)};
  const void *values[] = {vendor, product};
  CFDictionaryRef match = NULL;

  if (vendor && product) {
    match = CFDictionaryCreate(
        kCFAllocatorDefault, keys, values, 2, &kCFTypeDictionaryKeyCallBacks,
        &kCFTypeDictionaryValueCallBacks);
  }
  if (vendor) CFRelease(vendor);
  if (product) CFRelease(product);
  return match;
}

static void native_stop(void *context) {
  MuseOnNativeBackend *backend = context;

  if (!backend) return;
  backend->active = false;
  backend->emit = NULL;
  backend->emit_context = NULL;
  if (backend->manager) {
    IOHIDManagerUnscheduleFromRunLoop(backend->manager, CFRunLoopGetMain(),
                                      kCFRunLoopDefaultMode);
    IOHIDManagerClose(backend->manager, kIOHIDOptionsTypeNone);
    CFRelease(backend->manager);
    backend->manager = NULL;
  }
  if (backend->devices) {
    CFRelease(backend->devices);
    backend->devices = NULL;
  }
}

static bool native_start(void *context, MuseOnConnectionObserverEmit emit,
                         void *emit_context) {
  MuseOnNativeBackend *backend = context;
  CFDictionaryRef match;
  IOReturn result;
  CFSetRef current_devices;

  if (!backend || !emit || backend->active) return false;
  backend->devices = CFArrayCreateMutable(kCFAllocatorDefault, 0,
                                          &kCFTypeArrayCallBacks);
  backend->manager = IOHIDManagerCreate(kCFAllocatorDefault,
                                        kIOHIDManagerOptionNone);
  match = create_match_dictionary();
  if (!backend->devices || !backend->manager || !match) {
    if (match) CFRelease(match);
    native_stop(backend);
    return false;
  }
  backend->emit = emit;
  backend->emit_context = emit_context;
  backend->active = true;
  IOHIDManagerSetDeviceMatching(backend->manager, match);
  CFRelease(match);
  IOHIDManagerRegisterDeviceMatchingCallback(backend->manager, device_added,
                                             backend);
  IOHIDManagerRegisterDeviceRemovalCallback(backend->manager, device_removed,
                                             backend);
  IOHIDManagerScheduleWithRunLoop(backend->manager, CFRunLoopGetMain(),
                                  kCFRunLoopDefaultMode);
  result = IOHIDManagerOpen(backend->manager, kIOHIDOptionsTypeNone);
  if (result != kIOReturnSuccess) {
    native_stop(backend);
    return false;
  }
  current_devices = IOHIDManagerCopyDevices(backend->manager);
  if (current_devices) {
    CFIndex count = CFSetGetCount(current_devices);
    const void **values = calloc((size_t)count, sizeof(*values));
    CFIndex index;
    if (values) {
      CFSetGetValues(current_devices, values);
      for (index = 0; index < count; index++) {
        device_added(backend, kIOReturnSuccess, backend,
                     (IOHIDDeviceRef)values[index]);
      }
      free(values);
    } else {
      emit_snapshot(backend);
    }
    CFRelease(current_devices);
  } else {
    emit_snapshot(backend);
  }
  return true;
}

static void native_destroy(void *context) {
  MuseOnNativeBackend *backend = context;

  native_stop(backend);
  free(backend);
}

#ifdef MUSE_ON_CONNECTION_OBSERVER_TESTING
typedef MuseOnDeviceIdentity MuseOnConnectionObserverTestDevice;

MuseOnConnectionSnapshot muse_on_connection_observer_test_snapshot(
    const MuseOnConnectionObserverTestDevice *devices, size_t count) {
  return classify_identities(devices, count);
}
#endif

@interface MuseOnConnectionObserver ()
@property(nonatomic) MuseOnConnectionObserverBackend backend;
@property(nonatomic) MuseOnConnectionObserverCallback callback;
@property(nonatomic) void *callbackContext;
@property(nonatomic) BOOL started;
@property(nonatomic) NSUInteger generation;
- (void)emitSnapshot:(MuseOnConnectionSnapshot)snapshot;
@end

static void observer_emit(void *context, MuseOnConnectionSnapshot snapshot) {
  MuseOnConnectionObserver *observer =
      (__bridge MuseOnConnectionObserver *)context;
  [observer emitSnapshot:snapshot];
}

@implementation MuseOnConnectionObserver

- (instancetype)initWithBackend:(MuseOnConnectionObserverBackend)backend
                        callback:(MuseOnConnectionObserverCallback)callback
                 callbackContext:(void *)callbackContext {
  self = [super init];
  if (!self) return nil;
  _backend = backend;
  _callback = callback;
  _callbackContext = callbackContext;
  return self;
}

- (void)dealloc {
  [self stop];
  if (_backend.destroy) _backend.destroy(_backend.context);
}

- (BOOL)start {
  BOOL started;

  if (_started) return YES;
  if (!_backend.start || !_callback) return NO;
  _generation++;
  _started = YES;
  started = _backend.start(_backend.context, observer_emit, (__bridge void *)self);
  if (!started) {
    _started = NO;
    _generation++;
    if (_backend.stop) _backend.stop(_backend.context);
  }
  return started;
}

- (void)stop {
  if (!_started) return;
  _started = NO;
  _generation++;
  if (_backend.stop) _backend.stop(_backend.context);
}

- (void)emitSnapshot:(MuseOnConnectionSnapshot)snapshot {
  NSUInteger generation;
  MuseOnConnectionObserverCallback callback;
  void *callbackContext;

  if (!_started || !_callback) return;
  generation = _generation;
  callback = _callback;
  callbackContext = _callbackContext;
  dispatch_async(dispatch_get_main_queue(), ^{
    if (!self.started || self.generation != generation) return;
    callback(callbackContext, snapshot);
  });
}

@end

MuseOnConnectionObserver *muse_on_connection_observer_create_native(
    MuseOnConnectionObserverCallback callback, void *callback_context) {
  MuseOnNativeBackend *native = calloc(1, sizeof(*native));
  MuseOnConnectionObserverBackend backend;

  if (!native) return nil;
  backend = (MuseOnConnectionObserverBackend){native, native_start, native_stop,
                                              native_destroy};
  MuseOnConnectionObserver *observer = [[MuseOnConnectionObserver alloc]
      initWithBackend:backend callback:callback callbackContext:callback_context];
  if (!observer) native_destroy(native);
  return observer;
}
