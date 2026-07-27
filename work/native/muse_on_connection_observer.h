#ifndef MUSE_ON_CONNECTION_OBSERVER_H
#define MUSE_ON_CONNECTION_OBSERVER_H

#import <Foundation/Foundation.h>

#include <stdbool.h>

#include "muse_on_connection.h"

typedef void (*MuseOnConnectionObserverEmit)(
    void *context, MuseOnConnectionSnapshot snapshot);

typedef bool (*MuseOnConnectionObserverStart)(
    void *context, MuseOnConnectionObserverEmit emit, void *emit_context);
typedef void (*MuseOnConnectionObserverStop)(void *context);
typedef void (*MuseOnConnectionObserverDestroy)(void *context);

typedef struct {
  void *context;
  MuseOnConnectionObserverStart start;
  MuseOnConnectionObserverStop stop;
  MuseOnConnectionObserverDestroy destroy;
} MuseOnConnectionObserverBackend;

typedef void (*MuseOnConnectionObserverCallback)(
    void *context, MuseOnConnectionSnapshot snapshot);

@interface MuseOnConnectionObserver : NSObject
- (instancetype)initWithBackend:(MuseOnConnectionObserverBackend)backend
                        callback:(MuseOnConnectionObserverCallback)callback
                 callbackContext:(void *)callbackContext;
- (BOOL)start;
- (void)stop;
@end

/* Creates the production, read-only VID/PID observer backend. */
MuseOnConnectionObserver *muse_on_connection_observer_create_native(
    MuseOnConnectionObserverCallback callback, void *callback_context);

#endif
