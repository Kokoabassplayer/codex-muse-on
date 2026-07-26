#import <AppKit/AppKit.h>
#import <ServiceManagement/ServiceManagement.h>
#import <UserNotifications/UserNotifications.h>

#include <fcntl.h>
#include <sys/file.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include "muse_on_platform.h"
#include "muse_on_action_map.h"
#include "muse_on_connection.h"
#include "muse_on_diagnostics.h"
#include "muse_on_setup_state.h"
#include "muse_on_state_coordinator.h"

static NSString *const kEnabledIntentKey = @"enabledIntent";
static NSString *const kFirstEnableCompletedKey = @"firstEnableCompleted";
static NSString *const kStartAutomaticallyKey = @"startAutomatically";
static NSString *const kControlProfileKey = @"controlProfile";
static NSString *const kStartupApprovalRequiredKey = @"startupApprovalRequired";
static NSString *const kSafeQuitVerifiedKey = @"safeQuitVerified";
static NSString *const kDisablePendingKey = @"disablePending";

typedef enum {
  kListenerStopNone = 0,
  kListenerStopForProfile,
  kListenerStopForDisable,
  kListenerStopForRetry,
  kListenerStopForQuit,
  kListenerStopForSession
} ListenerStopPurpose;

static NSString *MuseOnProfileTitle(MuseOnProfile profile) {
  return profile == MUSE_ON_PROFILE_PEDAL ? @"Pedal Enabled"
                                          : @"Controller Only";
}

static NSString *MuseOnControlText(const MuseOnControlMapping *mapping,
                                   MuseOnProfile profile) {
  const MuseOnControlProfileMapping *profile_mapping =
      muse_on_control_mapping_profile(mapping, profile);
  NSString *physical = [NSString stringWithUTF8String:mapping->physical_label];

  if (!profile_mapping || !profile_mapping->available) {
    return [NSString stringWithFormat:
        @"%@ → Not active in %@; select Pedal Enabled to use it", physical,
        MuseOnProfileTitle(profile)];
  }
  return [NSString stringWithFormat:@"%@ → %s", physical,
                                    muse_on_action_display_name(
                                        profile_mapping->press_action)];
}

static NSRect MuseOnNormalizedRect(const MuseOnControlMapping *mapping,
                                   NSSize size) {
  return NSMakeRect(mapping->x * size.width, mapping->y * size.height,
                    mapping->width * size.width,
                    mapping->height * size.height);
}

@interface MuseOnControllerMapCanvas : NSView
@property(nonatomic) MuseOnProfile profile;
- (instancetype)initWithProfile:(MuseOnProfile)profile;
@end

@implementation MuseOnControllerMapCanvas

- (instancetype)initWithProfile:(MuseOnProfile)profile {
  self = [super initWithFrame:NSZeroRect];
  if (self) {
    _profile = profile;
    self.wantsLayer = YES;
    self.accessibilityRole = NSAccessibilityImageRole;
    self.accessibilityLabel = @"Muse-On physical controller layout";
    self.accessibilityValue = [NSString stringWithFormat:
        @"Selected profile: %@", MuseOnProfileTitle(profile)];
  }
  return self;
}

- (BOOL)isFlipped {
  return YES;
}

- (NSSize)intrinsicContentSize {
  return NSMakeSize(700, 260);
}

- (void)drawLabel:(NSString *)label
           atPoint:(NSPoint)point
              font:(NSFont *)font
             color:(NSColor *)color {
  [label drawAtPoint:point withAttributes:@{
    NSFontAttributeName : font,
    NSForegroundColorAttributeName : color,
  }];
}

- (void)drawRect:(NSRect)dirtyRect {
  (void)dirtyRect;
  NSSize size = self.bounds.size;
  NSRect panel = NSInsetRect(self.bounds, 1, 1);
  NSBezierPath *panelPath = [NSBezierPath bezierPathWithRoundedRect:panel
                                                              xRadius:12
                                                              yRadius:12];
  [[NSColor controlBackgroundColor] setFill];
  [panelPath fill];
  [[NSColor separatorColor] setStroke];
  [panelPath stroke];

  [self drawLabel:@"MUSE-ON" atPoint:NSMakePoint(18, 12)
              font:[NSFont systemFontOfSize:11 weight:NSFontWeightSemibold]
             color:[NSColor secondaryLabelColor]];
  [self drawLabel:[NSString stringWithFormat:@"%@ profile",
                                             MuseOnProfileTitle(_profile)]
          atPoint:NSMakePoint(size.width - 145, 12)
              font:[NSFont systemFontOfSize:11]
             color:[NSColor secondaryLabelColor]];

  NSRect turntable = NSMakeRect(0.045 * size.width, 0.20 * size.height,
                                0.18 * size.width, 0.57 * size.height);
  NSBezierPath *turntablePath = [NSBezierPath bezierPathWithOvalInRect:turntable];
  [[NSColor controlColor] setFill];
  [turntablePath fill];
  [[NSColor separatorColor] setStroke];
  [turntablePath stroke];
  NSBezierPath *inner = [NSBezierPath bezierPathWithOvalInRect:
      NSInsetRect(turntable, 15, 15)];
  [[NSColor windowBackgroundColor] setFill];
  [inner fill];
  [[NSColor separatorColor] setStroke];
  [inner stroke];
  [self drawLabel:@"↺" atPoint:NSMakePoint(NSMinX(turntable) + 17,
                                            NSMidY(turntable) - 13)
              font:[NSFont systemFontOfSize:25]
             color:[NSColor labelColor]];
  [self drawLabel:@"↻" atPoint:NSMakePoint(NSMaxX(turntable) - 39,
                                            NSMidY(turntable) - 13)
              font:[NSFont systemFontOfSize:25]
             color:[NSColor labelColor]];
  [self drawLabel:@"TURNTABLE" atPoint:NSMakePoint(NSMinX(turntable) + 25,
                                                   NSMaxY(turntable) + 8)
              font:[NSFont systemFontOfSize:9 weight:NSFontWeightSemibold]
             color:[NSColor secondaryLabelColor]];

  size_t index;
  size_t count = muse_on_control_mapping_count();
  for (index = 0; index < count; index++) {
    const MuseOnControlMapping *mapping = muse_on_control_mapping_at(index);
    NSRect frame = MuseOnNormalizedRect(mapping, size);
    NSString *shortLabel = nil;
    NSColor *fill = nil;
    NSColor *stroke = [NSColor separatorColor];

    if (mapping->group == MUSE_ON_CONTROL_GROUP_WHITE_BUTTONS) {
      fill = [NSColor controlColor];
      shortLabel = [NSString stringWithFormat:@"W%c", mapping->identifier[5]];
    } else if (mapping->group == MUSE_ON_CONTROL_GROUP_BLACK_BUTTONS) {
      fill = [NSColor textColor];
      shortLabel = [NSString stringWithFormat:@"B%c", mapping->identifier[5]];
      stroke = [NSColor textColor];
    } else if (mapping->group == MUSE_ON_CONTROL_GROUP_OPTIONAL_PEDAL) {
      BOOL selected = muse_on_control_mapping_profile(mapping, _profile)->available;
      fill = selected ? [NSColor controlAccentColor] : [NSColor controlColor];
      shortLabel = selected ? @"PEDAL" : @"PEDAL (off)";
    }
    if (fill) {
      NSBezierPath *button = [NSBezierPath bezierPathWithRoundedRect:frame
                                                               xRadius:5
                                                               yRadius:5];
      [fill setFill];
      [button fill];
      [stroke setStroke];
      [button stroke];
      [self drawLabel:shortLabel
              atPoint:NSMakePoint(NSMidX(frame) - 12, NSMidY(frame) - 6)
                  font:[NSFont systemFontOfSize:10 weight:NSFontWeightSemibold]
                 color:mapping->group == MUSE_ON_CONTROL_GROUP_BLACK_BUTTONS
                     ? [NSColor controlBackgroundColor] : [NSColor labelColor]];
    }
  }

  NSPoint leftCenter = NSMakePoint(0.78 * size.width, 0.43 * size.height);
  NSPoint rightCenter = NSMakePoint(0.90 * size.width, 0.43 * size.height);
  for (NSValue *value in @[[NSValue valueWithPoint:leftCenter],
                           [NSValue valueWithPoint:rightCenter]]) {
    NSPoint center = value.pointValue;
    NSRect ball = NSMakeRect(center.x - 28, center.y - 28, 56, 56);
    NSBezierPath *ballPath = [NSBezierPath bezierPathWithOvalInRect:ball];
    [[NSColor controlColor] setFill];
    [ballPath fill];
    [[NSColor separatorColor] setStroke];
    [ballPath stroke];
    NSBezierPath *dot = [NSBezierPath bezierPathWithOvalInRect:
        NSMakeRect(center.x - 5, center.y - 5, 10, 10)];
    [[NSColor controlAccentColor] setFill];
    [dot fill];
  }
  [self drawLabel:@"N" atPoint:NSMakePoint(leftCenter.x - 4, leftCenter.y - 50)
              font:[NSFont systemFontOfSize:10 weight:NSFontWeightSemibold]
             color:[NSColor labelColor]];
  [self drawLabel:@"S" atPoint:NSMakePoint(leftCenter.x - 4, leftCenter.y + 35)
              font:[NSFont systemFontOfSize:10 weight:NSFontWeightSemibold]
             color:[NSColor labelColor]];
  [self drawLabel:@"←  ↑  →" atPoint:NSMakePoint(rightCenter.x - 24,
                                                     rightCenter.y + 38)
              font:[NSFont systemFontOfSize:10 weight:NSFontWeightSemibold]
             color:[NSColor labelColor]];
  [self drawLabel:@"BALLS" atPoint:NSMakePoint(0.80 * size.width,
                                                0.78 * size.height)
              font:[NSFont systemFontOfSize:9 weight:NSFontWeightSemibold]
             color:[NSColor secondaryLabelColor]];
}

@end

@interface MuseOnControllerMapView : NSView
- (instancetype)initWithProfile:(MuseOnProfile)profile;
@end

@implementation MuseOnControllerMapView

- (instancetype)initWithProfile:(MuseOnProfile)profile {
  self = [super initWithFrame:NSMakeRect(0, 0, 700, 620)];
  if (!self) return nil;
  self.translatesAutoresizingMaskIntoConstraints = NO;
  self.accessibilityRole = NSAccessibilityGroupRole;
  self.accessibilityLabel = @"Read-only Controller Map";

  NSStackView *stack = [NSStackView stackViewWithViews:@[]];
  stack.orientation = NSUserInterfaceLayoutOrientationVertical;
  stack.alignment = NSLayoutAttributeLeading;
  stack.spacing = 7;
  stack.translatesAutoresizingMaskIntoConstraints = NO;
  [self addSubview:stack];

  MuseOnControllerMapCanvas *canvas =
      [[MuseOnControllerMapCanvas alloc] initWithProfile:profile];
  canvas.translatesAutoresizingMaskIntoConstraints = NO;
  [stack addArrangedSubview:canvas];
  [canvas.widthAnchor constraintEqualToAnchor:stack.widthAnchor].active = YES;
  [canvas.heightAnchor constraintEqualToConstant:260].active = YES;

  NSTextField *equivalentTitle = [NSTextField labelWithString:
      @"Complete text equivalent — physical control → Codex action"];
  equivalentTitle.font = [NSFont systemFontOfSize:12 weight:NSFontWeightSemibold];
  equivalentTitle.accessibilityLabel = equivalentTitle.stringValue;
  [stack addArrangedSubview:equivalentTitle];

  for (NSInteger groupValue = 0;
       groupValue < MUSE_ON_CONTROL_GROUP_COUNT; groupValue++) {
    MuseOnControlGroup group = (MuseOnControlGroup)groupValue;
    NSStackView *groupStack = [NSStackView stackViewWithViews:@[]];
    groupStack.orientation = NSUserInterfaceLayoutOrientationVertical;
    groupStack.alignment = NSLayoutAttributeLeading;
    groupStack.spacing = 2;
    groupStack.accessibilityRole = NSAccessibilityGroupRole;
    groupStack.accessibilityLabel = [NSString stringWithUTF8String:
        muse_on_control_group_title(group)];
    groupStack.translatesAutoresizingMaskIntoConstraints = NO;

    NSTextField *heading = [NSTextField labelWithString:
        [NSString stringWithFormat:@"%@", groupStack.accessibilityLabel]];
    heading.font = [NSFont systemFontOfSize:11 weight:NSFontWeightSemibold];
    heading.textColor = [NSColor secondaryLabelColor];
    [groupStack addArrangedSubview:heading];

    size_t count = muse_on_control_mapping_count();
    for (size_t index = 0; index < count; index++) {
      const MuseOnControlMapping *mapping =
          muse_on_control_mapping_at(index);
      if (mapping->group != group) continue;
      NSTextField *row = [NSTextField labelWithString:
          MuseOnControlText(mapping, profile)];
      row.font = [NSFont systemFontOfSize:11];
      row.accessibilityRole = NSAccessibilityStaticTextRole;
      row.accessibilityLabel = row.stringValue;
      row.lineBreakMode = NSLineBreakByWordWrapping;
      row.usesSingleLineMode = NO;
      row.preferredMaxLayoutWidth = 680;
      [groupStack addArrangedSubview:row];
    }
    [stack addArrangedSubview:groupStack];
  }

  [NSLayoutConstraint activateConstraints:@[
      [stack.leadingAnchor constraintEqualToAnchor:self.leadingAnchor],
      [stack.trailingAnchor constraintEqualToAnchor:self.trailingAnchor],
      [stack.topAnchor constraintEqualToAnchor:self.topAnchor],
      [stack.bottomAnchor constraintEqualToAnchor:self.bottomAnchor],
  ]];
  return self;
}

@end

@interface MuseOnAppDelegate : NSObject <NSApplicationDelegate>
@property(nonatomic, strong) NSStatusItem *statusItem;
@property(nonatomic, strong) NSPopover *popover;
@property(nonatomic, strong) NSTask *listenerTask;
@property(nonatomic, strong) NSFileHandle *listenerOutputHandle;
@property(nonatomic, strong) NSMutableDictionary<NSString *, NSDictionary *> *interfaces;
@property(nonatomic, strong) NSMutableData *listenerOutputBuffer;
@property(nonatomic) BOOL controllerConnected;
@property(nonatomic) BOOL multipleControllers;
@property(nonatomic) BOOL inputsReleased;
@property(nonatomic) BOOL permissionGranted;
@property(nonatomic) BOOL filterVerified;
@property(nonatomic) BOOL sessionAvailable;
@property(nonatomic) BOOL codexForeground;
@property(nonatomic) BOOL listenerRecoveryMode;
@property(nonatomic) BOOL listenerRecoveryValidated;
@property(nonatomic) BOOL listenerCleanupVerified;
@property(nonatomic) BOOL listenerSawStopped;
@property(nonatomic) BOOL listenerSawSafetyLatch;
@property(nonatomic) MuseOnSafetyFailure listenerSafetyFailure;
@property(nonatomic) ListenerStopPurpose listenerStopPurpose;
@property(nonatomic) BOOL quitInFlight;
@property(nonatomic) BOOL listenerEverStarted;
@property(nonatomic) BOOL listenerCleanupKnown;
@property(nonatomic) BOOL primaryInstance;
@property(nonatomic) MuseOnDiagnostics diagnostics;
@property(nonatomic) MuseOnSetupState setup;
@property(nonatomic) MuseOnState coordinator;
@property(nonatomic) MuseOnProfile profile;
@property(nonatomic) int lockFd;
- (void)applyCoordinatorCommand:(MuseOnCommand)command
               cleanupVerified:(BOOL)cleanupVerified
                 safetyFailure:(MuseOnSafetyFailure)safetyFailure;
- (void)notifySafetyLatchIfAuthorized;
- (void)finishSafeQuit;
- (void)refreshStatusIcon;
@end

@implementation MuseOnAppDelegate

- (BOOL)acquirePrimaryInstanceLock {
  NSFileManager *manager = [NSFileManager defaultManager];
  NSURL *support = [[manager URLsForDirectory:NSApplicationSupportDirectory
                                     inDomains:NSUserDomainMask] firstObject];
  NSURL *directory = [support URLByAppendingPathComponent:@"Codex Muse-On"
                                               isDirectory:YES];
  NSError *error = nil;
  if (![manager createDirectoryAtURL:directory withIntermediateDirectories:YES
                         attributes:nil error:&error]) {
    return NO;
  }
  NSString *path = [[directory URLByAppendingPathComponent:@"primary.lock"] path];
  self.lockFd = open(path.fileSystemRepresentation, O_CREAT | O_RDWR, 0600);
  return self.lockFd >= 0 && flock(self.lockFd, LOCK_EX | LOCK_NB) == 0;
}

- (void)loadSetup {
  NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
  id safeQuitMarker = [defaults objectForKey:kSafeQuitVerifiedKey];
  BOOL previousExitWasUnclean = safeQuitMarker != nil &&
                                ![safeQuitMarker boolValue];
  BOOL disablePending = [defaults boolForKey:kDisablePendingKey];
  muse_on_setup_state_init(&_setup, [defaults boolForKey:kEnabledIntentKey],
                          [defaults boolForKey:kStartAutomaticallyKey]);
  muse_on_state_init(&_coordinator);
  muse_on_diagnostics_init(&_diagnostics);
  _sessionAvailable = muse_on_session_is_available();
  _permissionGranted = muse_on_preflight_post_event_access();
  _inputsReleased = true;
  _filterVerified = true;
  _listenerRecoveryValidated = NO;
  _listenerEverStarted = NO;
  _listenerCleanupKnown = NO;
  [defaults setBool:NO forKey:kSafeQuitVerifiedKey];
  _listenerSafetyFailure = previousExitWasUnclean &&
                           (_setup.enabled || disablePending)
      ? MUSE_ON_SAFETY_FAILURE_UNCLEAN_EXIT
      : (disablePending ? MUSE_ON_SAFETY_FAILURE_DEVICE_UNCERTAIN
                        : MUSE_ON_SAFETY_FAILURE_NONE);
  if (_listenerSafetyFailure == MUSE_ON_SAFETY_FAILURE_UNCLEAN_EXIT) {
    muse_on_diagnostics_record_error(
        &_diagnostics, MUSE_ON_DIAGNOSTIC_ERROR_UNCLEAN_EXIT);
  } else if (_listenerSafetyFailure == MUSE_ON_SAFETY_FAILURE_DEVICE_UNCERTAIN) {
    muse_on_diagnostics_record_error(
        &_diagnostics, MUSE_ON_DIAGNOSTIC_ERROR_DEVICE_UNCERTAIN);
  }
  _profile = [[defaults stringForKey:kControlProfileKey] isEqualToString:@"pedal"]
      ? MUSE_ON_PROFILE_PEDAL : MUSE_ON_PROFILE_CONTROLLER_ONLY;
}

- (NSUInteger)completeControllerCount {
  NSMutableSet<NSNumber *> *locations = [NSMutableSet set];
  NSUInteger complete = 0;

  for (NSDictionary *entry in self.interfaces.allValues) {
    NSNumber *location = entry[@"location"];
    if (location) [locations addObject:location];
  }
  for (NSNumber *location in locations) {
    NSUInteger keyboards = 0;
    NSUInteger joysticks = 0;
    for (NSDictionary *entry in self.interfaces.allValues) {
      if (![entry[@"location"] isEqual:location]) continue;
      if ([entry[@"kind"] integerValue] == MUSE_ON_OBSERVED_KEYBOARD) {
        keyboards++;
      } else if ([entry[@"kind"] integerValue] == MUSE_ON_OBSERVED_JOYSTICK) {
        joysticks++;
      }
    }
    if (keyboards == 1 && joysticks == 1) complete++;
  }
  return complete;
}

- (void)refreshControllerObservation {
  NSUInteger complete = [self completeControllerCount];
  self.controllerConnected = [self hasSingleCompleteController];
  self.multipleControllers = complete > 1;
}

- (void)sessionDidChange:(NSNotification *)notification {
  (void)notification;
  self.sessionAvailable = muse_on_session_is_available();
  if (!self.sessionAvailable && self.listenerTask &&
      !self.listenerRecoveryMode &&
      self.listenerStopPurpose == kListenerStopNone) {
    self.listenerStopPurpose = kListenerStopForSession;
    [self.listenerTask terminate];
  }
  [self updateCoordinatorWithCommand:MUSE_ON_COMMAND_NONE];
  if (self.sessionAvailable && self.listenerTask == nil &&
      _setup.enabled && !_coordinator.safety_latched) {
    [self startListenerIfNeeded];
  }
  if (self.popover.shown) [self refreshMenu];
}

- (void)saveSetup {
  NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
  [defaults setBool:_setup.enabled forKey:kEnabledIntentKey];
  [defaults setBool:_setup.start_automatically forKey:kStartAutomaticallyKey];
}

- (BOOL)hasSingleCompleteController {
  NSArray<NSDictionary *> *observed = self.interfaces.allValues;
  MuseOnObservedInterface *interfaces;
  size_t count = observed.count;
  size_t index;
  uint32_t locationID = 0;
  BOOL connected;

  if (count == 0) return NO;
  interfaces = calloc(count, sizeof(*interfaces));
  if (!interfaces) return NO;
  for (index = 0; index < count; index++) {
    NSDictionary *entry = observed[index];
    interfaces[index] = (MuseOnObservedInterface){
        [entry[@"kind"] integerValue], [entry[@"location"] unsignedIntValue],
        true};
  }
  connected = muse_on_find_single_complete_controller(interfaces, count,
                                                       &locationID);
  free(interfaces);
  return connected;
}

- (MuseOnSafetyFailure)safetyFailureFromString:(NSString *)value {
  const char *raw = value.UTF8String;

  if (!raw) return MUSE_ON_SAFETY_FAILURE_DEVICE_UNCERTAIN;
  if (strcmp(raw, "hold_release_failed") == 0) {
    return MUSE_ON_SAFETY_FAILURE_HOLD_RELEASE;
  }
  if (strcmp(raw, "pass_through_restoration_failed") == 0) {
    return MUSE_ON_SAFETY_FAILURE_PASSTHROUGH_RESTORE;
  }
  if (strcmp(raw, "previous_session_ended_unexpectedly") == 0) {
    return MUSE_ON_SAFETY_FAILURE_UNCLEAN_EXIT;
  }
  return MUSE_ON_SAFETY_FAILURE_DEVICE_UNCERTAIN;
}

- (void)recordListenerError:(NSString *)operation {
  const char *raw = operation.UTF8String;
  MuseOnDiagnosticErrorCode error = MUSE_ON_DIAGNOSTIC_ERROR_DEVICE_UNCERTAIN;

  if (!raw) return;
  if (strcmp(raw, "restore_keyboard_filter") == 0 ||
      strcmp(raw, "rollback_keyboard_filter") == 0 ||
      strcmp(raw, "recover_keyboard_filter") == 0) {
    error = MUSE_ON_DIAGNOSTIC_ERROR_FILTER_RESTORE;
  } else if (strcmp(raw, "apply_keyboard_filter") == 0) {
    error = MUSE_ON_DIAGNOSTIC_ERROR_FILTER_APPLY;
  } else if (strcmp(raw, "input_report") == 0 ||
             strcmp(raw, "invalid_input_report") == 0) {
    return;
  }
  muse_on_diagnostics_record_error(&_diagnostics, error);
}

- (void)recordSafetyFailure:(MuseOnSafetyFailure)failure {
  MuseOnDiagnosticErrorCode error;

  switch (failure) {
    case MUSE_ON_SAFETY_FAILURE_HOLD_RELEASE:
      error = MUSE_ON_DIAGNOSTIC_ERROR_HOLD_RELEASE;
      break;
    case MUSE_ON_SAFETY_FAILURE_PASSTHROUGH_RESTORE:
      error = MUSE_ON_DIAGNOSTIC_ERROR_FILTER_RESTORE;
      break;
    case MUSE_ON_SAFETY_FAILURE_UNCLEAN_EXIT:
      error = MUSE_ON_DIAGNOSTIC_ERROR_UNCLEAN_EXIT;
      break;
    case MUSE_ON_SAFETY_FAILURE_DEVICE_UNCERTAIN:
      error = MUSE_ON_DIAGNOSTIC_ERROR_DEVICE_UNCERTAIN;
      break;
    case MUSE_ON_SAFETY_FAILURE_NONE:
      return;
  }
  muse_on_diagnostics_record_error(&_diagnostics, error);
}

- (void)recordListenerEvent:(NSDictionary *)event {
  NSString *name = event[@"event"];
  NSString *actionName = event[@"action"];
  NSString *operation = event[@"operation"];
  NSString *safetyReason = event[@"reason"];

  if ([name isEqualToString:@"tcc_status"]) {
    NSString *inputMonitoring = event[@"inputMonitoring"];
    NSNumber *accessibility = event[@"accessibility"];
    if (inputMonitoring) {
      self.permissionGranted =
          [inputMonitoring isEqualToString:@"granted"];
    }
    if (accessibility) self.permissionGranted &= accessibility.boolValue;
    [self updateCoordinatorWithCommand:MUSE_ON_COMMAND_NONE];
  } else if ([name isEqualToString:@"ready"]) {
    NSString *inputMonitoring = event[@"inputMonitoring"];
    NSNumber *accessibility = event[@"accessibility"];
    if (inputMonitoring && accessibility) {
      self.permissionGranted =
          [inputMonitoring isEqualToString:@"granted"] &&
          accessibility.boolValue;
    }
    if (event[@"keyFilterApplied"]) {
      self.filterVerified = [event[@"keyFilterApplied"] boolValue];
    }
    [self updateCoordinatorWithCommand:MUSE_ON_COMMAND_NONE];
  } else if ([name isEqualToString:@"focus_changed"]) {
    self.codexForeground = [event[@"codexFrontmost"] boolValue];
    if (!self.codexForeground) self.inputsReleased = NO;
    [self updateCoordinatorWithCommand:MUSE_ON_COMMAND_NONE];
  } else if ([name isEqualToString:@"filter_applied"]) {
    self.filterVerified = [event[@"keyFilterApplied"] boolValue];
    [self updateCoordinatorWithCommand:MUSE_ON_COMMAND_NONE];
  } else if ([name isEqualToString:@"filter_restored"]) {
    self.filterVerified = NO;
    [self updateCoordinatorWithCommand:MUSE_ON_COMMAND_NONE];
  } else if ([name isEqualToString:@"neutral_entry"]) {
    self.inputsReleased = [event[@"inputsReleased"] boolValue];
    [self updateCoordinatorWithCommand:MUSE_ON_COMMAND_NONE];
  }

  if ([name isEqualToString:@"action_dispatched"] ||
      [name isEqualToString:@"action_dispatch_failed"] ||
      [name isEqualToString:@"action_blocked"]) {
    for (int value = 0; value < MUSE_ON_ACTION_COUNT; value++) {
      if ([actionName isEqualToString:@(muse_on_action_id_string(
                                            (MuseOnActionId)value))]) {
        muse_on_diagnostics_record_action(&_diagnostics,
                                          (MuseOnActionId)value);
        break;
      }
    }
  } else if ([name isEqualToString:@"error"]) {
    [self recordListenerError:operation];
  } else if ([name isEqualToString:@"safety_latch"]) {
    self.listenerSawSafetyLatch = YES;
    self.listenerSafetyFailure = [self safetyFailureFromString:safetyReason];
    [self recordSafetyFailure:self.listenerSafetyFailure];
    [self applyCoordinatorCommand:MUSE_ON_COMMAND_NONE
                 cleanupVerified:NO
                   safetyFailure:self.listenerSafetyFailure];
    if (self.popover.shown) [self refreshMenu];
  } else if ([name isEqualToString:@"recovery_state"]) {
    self.listenerRecoveryValidated = YES;
    self.controllerConnected = [event[@"controllerConnected"] boolValue];
    self.multipleControllers = [event[@"multipleControllers"] boolValue];
    self.inputsReleased = [event[@"inputsReleased"] boolValue];
    self.permissionGranted = [event[@"permissionGranted"] boolValue];
    self.filterVerified = [event[@"filterVerified"] boolValue];
    self.codexForeground = [event[@"codexForeground"] boolValue];
    if (self.listenerRecoveryMode && self.listenerTask != nil &&
        (self.listenerStopPurpose == kListenerStopForRetry ||
         self.listenerStopPurpose == kListenerStopForDisable)) {
      [self.listenerTask terminate];
    }
  } else if ([name isEqualToString:@"stopped"]) {
    self.listenerSawStopped = YES;
    self.listenerCleanupVerified = ![event[@"releaseFailed"] boolValue] &&
                                   (!safetyReason ||
                                    [safetyReason isEqualToString:@"none"]);
    if (safetyReason && ![safetyReason isEqualToString:@"none"]) {
      self.listenerSafetyFailure = [self safetyFailureFromString:safetyReason];
      self.listenerSawSafetyLatch = YES;
    }
  }
}

- (void)applyCoordinatorCommand:(MuseOnCommand)command
               cleanupVerified:(BOOL)cleanupVerified
                 safetyFailure:(MuseOnSafetyFailure)safetyFailure {
  MuseOnSafetyFailure effectiveFailure = safetyFailure;
  MuseOnState previous = _coordinator;
  self.sessionAvailable = muse_on_session_is_available();
  self.codexForeground = muse_on_codex_is_frontmost();
  if (command == MUSE_ON_COMMAND_RETRY &&
      self.listenerRecoveryValidated &&
      effectiveFailure == MUSE_ON_SAFETY_FAILURE_NONE &&
      self.permissionGranted && self.controllerConnected &&
      !self.multipleControllers && self.sessionAvailable &&
      self.codexForeground && self.inputsReleased && !self.filterVerified) {
    effectiveFailure = MUSE_ON_SAFETY_FAILURE_PASSTHROUGH_RESTORE;
  }
  MuseOnPrerequisites prerequisites = {
      .safety_latched = effectiveFailure != MUSE_ON_SAFETY_FAILURE_NONE,
      .safety_failure = effectiveFailure,
      .cleanup_verified = cleanupVerified,
      .permission_granted = self.permissionGranted,
      .controller_connected = self.controllerConnected,
      .multiple_controllers = self.multipleControllers,
      .session_available = self.sessionAvailable,
      .codex_foreground = self.codexForeground,
      .inputs_released = self.inputsReleased,
  };

  muse_on_state_apply(&_coordinator, command, prerequisites);
  [self refreshStatusIcon];
  if (_coordinator.disable_pending) {
    [[NSUserDefaults standardUserDefaults] setBool:YES forKey:kDisablePendingKey];
  } else if (_coordinator.status == MUSE_ON_STATUS_DISABLED) {
    [[NSUserDefaults standardUserDefaults] setBool:NO forKey:kDisablePendingKey];
  }
  if (previous.status != _coordinator.status ||
      previous.inactive_reason != _coordinator.inactive_reason ||
      previous.safety_failure != _coordinator.safety_failure ||
      previous.disable_pending != _coordinator.disable_pending) {
    muse_on_diagnostics_record_state(
        &_diagnostics, _coordinator.status, _coordinator.inactive_reason,
        _coordinator.safety_failure);
  }
  if (previous.status != MUSE_ON_STATUS_SAFETY_LATCH &&
      _coordinator.status == MUSE_ON_STATUS_SAFETY_LATCH) {
    [self notifySafetyLatchIfAuthorized];
  }
}

- (void)consumeListenerData:(NSData *)data {
  [self.listenerOutputBuffer appendData:data];
  for (;;) {
    const uint8_t *bytes = self.listenerOutputBuffer.bytes;
    NSUInteger length = self.listenerOutputBuffer.length;
    NSUInteger index;
    NSRange newline = NSMakeRange(NSNotFound, 0);
    for (index = 0; index < length; index++) {
      if (bytes[index] == '\n') {
        newline = NSMakeRange(index, 1);
        break;
      }
    }
    if (newline.location == NSNotFound) break;
    NSData *line = [self.listenerOutputBuffer subdataWithRange:
        NSMakeRange(0, newline.location)];
    [self.listenerOutputBuffer replaceBytesInRange:
        NSMakeRange(0, newline.location + 1) withBytes:NULL length:0];
    NSDictionary *event = [NSJSONSerialization JSONObjectWithData:line
                                                           options:0 error:nil];
    if (![event isKindOfClass:[NSDictionary class]]) continue;
    NSString *name = event[@"event"];
    NSString *interface = event[@"interface"];
    NSNumber *location = event[@"locationID"];
    [self recordListenerEvent:event];
    if (([name isEqualToString:@"device_added"] ||
         [name isEqualToString:@"device_removed"]) && interface && location) {
      MuseOnObservedInterfaceKind kind = [interface isEqualToString:@"keyboard"]
          ? MUSE_ON_OBSERVED_KEYBOARD : MUSE_ON_OBSERVED_JOYSTICK;
      NSString *key = [NSString stringWithFormat:@"%@:%@", interface, location];
      if ([name isEqualToString:@"device_added"]) {
        self.interfaces[key] = @{@"kind": @(kind), @"location": location};
      } else {
        [self.interfaces removeObjectForKey:key];
      }
      [self refreshControllerObservation];
      self.inputsReleased = NO;
      self.filterVerified = NO;
      [self updateCoordinatorWithCommand:MUSE_ON_COMMAND_NONE];
      if (self.popover.shown) [self refreshMenu];
    }
  }
}

- (void)handleListenerTerminationForPurpose:(ListenerStopPurpose)purpose
                            cleanupVerified:(BOOL)cleanupVerified {
  BOOL recoveryMode = self.listenerRecoveryMode;
  MuseOnSafetyFailure failure = self.listenerSafetyFailure;

  if (purpose == kListenerStopForRetry && recoveryMode &&
      !self.listenerRecoveryValidated) {
    cleanupVerified = NO;
  }
  if (!cleanupVerified && failure == MUSE_ON_SAFETY_FAILURE_NONE) {
    failure = MUSE_ON_SAFETY_FAILURE_DEVICE_UNCERTAIN;
    self.listenerSafetyFailure = failure;
  }
  if (!cleanupVerified) {
    muse_on_diagnostics_record_error(&_diagnostics,
                                     MUSE_ON_DIAGNOSTIC_ERROR_LISTENER_EXIT);
  }

  switch (purpose) {
    case kListenerStopForProfile:
      if (cleanupVerified && _setup.enabled &&
          !_coordinator.safety_latched && !_coordinator.disable_pending) {
        [self startListenerIfNeeded];
      } else if (!cleanupVerified) {
        [self applyCoordinatorCommand:MUSE_ON_COMMAND_NONE
                     cleanupVerified:NO
                       safetyFailure:failure];
      }
      break;
    case kListenerStopForDisable:
      if (cleanupVerified) {
        self.listenerSafetyFailure = MUSE_ON_SAFETY_FAILURE_NONE;
        [self applyCoordinatorCommand:MUSE_ON_COMMAND_DISABLE
                     cleanupVerified:YES
                       safetyFailure:MUSE_ON_SAFETY_FAILURE_NONE];
      } else {
        [self applyCoordinatorCommand:MUSE_ON_COMMAND_DISABLE
                     cleanupVerified:NO
                       safetyFailure:failure];
      }
      break;
    case kListenerStopForRetry:
      if (!self.listenerRecoveryValidated && !recoveryMode &&
          !self.quitInFlight) {
        self.listenerStopPurpose = kListenerStopForRetry;
        [self startListenerWithSafetyLatch:YES];
      } else if (cleanupVerified) {
        self.listenerSafetyFailure = MUSE_ON_SAFETY_FAILURE_NONE;
        [self applyCoordinatorCommand:MUSE_ON_COMMAND_RETRY
                     cleanupVerified:YES
                       safetyFailure:MUSE_ON_SAFETY_FAILURE_NONE];
        if (_coordinator.quit_allowed) {
          [self finishSafeQuit];
          [NSApp replyToApplicationShouldTerminate:YES];
        } else if (_coordinator.status != MUSE_ON_STATUS_DISABLED &&
                   !_coordinator.safety_latched && _setup.enabled) {
          [self startListenerIfNeeded];
        }
      } else {
        [self applyCoordinatorCommand:MUSE_ON_COMMAND_RETRY
                     cleanupVerified:NO
                       safetyFailure:failure];
        if (self.quitInFlight) {
          self.quitInFlight = NO;
          [NSApp replyToApplicationShouldTerminate:NO];
        }
      }
      break;
    case kListenerStopForQuit:
      if (cleanupVerified) {
        self.listenerSafetyFailure = MUSE_ON_SAFETY_FAILURE_NONE;
        [self applyCoordinatorCommand:MUSE_ON_COMMAND_RETRY
                     cleanupVerified:YES
                       safetyFailure:MUSE_ON_SAFETY_FAILURE_NONE];
        if (_coordinator.quit_allowed) {
          [self finishSafeQuit];
          [NSApp replyToApplicationShouldTerminate:YES];
        } else {
          self.quitInFlight = NO;
          [NSApp replyToApplicationShouldTerminate:NO];
        }
      } else {
        [self applyCoordinatorCommand:MUSE_ON_COMMAND_NONE
                     cleanupVerified:NO
                       safetyFailure:failure];
        self.quitInFlight = NO;
        [NSApp replyToApplicationShouldTerminate:NO];
      }
      break;
    case kListenerStopForSession:
      if (cleanupVerified) {
        [self applyCoordinatorCommand:MUSE_ON_COMMAND_NONE
                     cleanupVerified:YES
                       safetyFailure:MUSE_ON_SAFETY_FAILURE_NONE];
        if (self.sessionAvailable && _setup.enabled &&
            !_coordinator.safety_latched) {
          [self startListenerIfNeeded];
        }
      } else {
        [self applyCoordinatorCommand:MUSE_ON_COMMAND_NONE
                     cleanupVerified:NO
                       safetyFailure:failure];
      }
      break;
    case kListenerStopNone:
      if (cleanupVerified) failure = MUSE_ON_SAFETY_FAILURE_DEVICE_UNCERTAIN;
      [self applyCoordinatorCommand:MUSE_ON_COMMAND_NONE
                   cleanupVerified:cleanupVerified
                     safetyFailure:failure];
      break;
  }
  if (self.popover.shown) [self refreshMenu];
  if (self.listenerTask == nil) {
    self.listenerStopPurpose = kListenerStopNone;
    self.listenerRecoveryMode = NO;
    self.listenerRecoveryValidated = NO;
  } else if (!self.listenerRecoveryMode) {
    self.listenerStopPurpose = kListenerStopNone;
    self.listenerRecoveryValidated = NO;
  }
}

- (void)startListenerWithSafetyLatch:(BOOL)safetyLatched {
  NSString *path;
  NSTask *task;
  NSMutableArray<NSString *> *arguments;
  if (self.listenerTask != nil ||
      (!safetyLatched && (!_setup.enabled || _coordinator.safety_latched ||
                          _coordinator.disable_pending))) return;
  path = [[NSBundle mainBundle] pathForAuxiliaryExecutable:@"muse_on_listener"];
  if (!path) {
    self.listenerSafetyFailure = MUSE_ON_SAFETY_FAILURE_DEVICE_UNCERTAIN;
    [self applyCoordinatorCommand:MUSE_ON_COMMAND_NONE
                 cleanupVerified:NO
                   safetyFailure:self.listenerSafetyFailure];
    return;
  }
  task = [[NSTask alloc] init];
  task.launchPath = path;
  arguments = [NSMutableArray arrayWithObjects:@"--mode=active",
      _profile == MUSE_ON_PROFILE_PEDAL ? @"--profile=pedal"
                                        : @"--profile=controller-only", nil];
  if (safetyLatched) [arguments addObject:@"--safety-latched"];
  task.arguments = arguments;
  self.listenerRecoveryMode = safetyLatched;
  self.listenerRecoveryValidated = NO;
  self.listenerCleanupVerified = NO;
  self.listenerSawStopped = NO;
  self.listenerSawSafetyLatch = NO;
  self.interfaces = [NSMutableDictionary dictionary];
  self.listenerOutputBuffer = [NSMutableData data];
  self.controllerConnected = NO;
  self.multipleControllers = NO;
  self.inputsReleased = true;
  self.filterVerified = NO;
  NSPipe *output = [NSPipe pipe];
  task.standardOutput = output;
  __weak MuseOnAppDelegate *weakSelf = self;
  output.fileHandleForReading.readabilityHandler = ^(NSFileHandle *handle) {
    NSData *data = handle.availableData;
    if (data.length == 0) {
      handle.readabilityHandler = nil;
      return;
    }
    dispatch_async(dispatch_get_main_queue(), ^{
      [weakSelf consumeListenerData:data];
    });
  };
  self.listenerOutputHandle = output.fileHandleForReading;
  task.terminationHandler = ^(NSTask *finishedTask) {
    dispatch_async(dispatch_get_main_queue(), ^{
      BOOL cleanupVerified;
      ListenerStopPurpose purpose;

      if (weakSelf.listenerTask != finishedTask) return;
      weakSelf.listenerOutputHandle.readabilityHandler = nil;
      NSData *remaining = [weakSelf.listenerOutputHandle availableData];
      if (remaining.length > 0) [weakSelf consumeListenerData:remaining];
      cleanupVerified = weakSelf.listenerSawStopped &&
                        weakSelf.listenerCleanupVerified;
      if (finishedTask.terminationStatus != 0) cleanupVerified = NO;
      if (!cleanupVerified && !weakSelf.listenerSawSafetyLatch) {
        weakSelf.listenerSafetyFailure = MUSE_ON_SAFETY_FAILURE_DEVICE_UNCERTAIN;
      }
      purpose = weakSelf.listenerStopPurpose;
      if (purpose != kListenerStopNone) {
        weakSelf.listenerCleanupKnown = cleanupVerified;
      }
      weakSelf.listenerTask = nil;
      [weakSelf handleListenerTerminationForPurpose:purpose
                                   cleanupVerified:cleanupVerified];
    });
  };
  @try {
    [task launch];
    self.listenerTask = task;
    self.listenerEverStarted = YES;
    self.listenerCleanupKnown = NO;
  } @catch (__unused NSException *exception) {
    self.listenerTask = nil;
    self.listenerRecoveryMode = NO;
    self.listenerSafetyFailure = MUSE_ON_SAFETY_FAILURE_DEVICE_UNCERTAIN;
    [self applyCoordinatorCommand:MUSE_ON_COMMAND_NONE
                 cleanupVerified:NO
                   safetyFailure:self.listenerSafetyFailure];
  }
}

- (void)startListenerIfNeeded {
  [self startListenerWithSafetyLatch:NO];
}

- (void)updateCoordinatorWithCommand:(MuseOnCommand)command {
  [self applyCoordinatorCommand:command
               cleanupVerified:YES
                 safetyFailure:self.listenerSafetyFailure];
}

- (NSString *)controlStatusText {
  if (_coordinator.status == MUSE_ON_STATUS_DISABLED) return @"Control: Disabled";
  if (_coordinator.status == MUSE_ON_STATUS_ACTIVE) return @"Control: Active";
  if (_coordinator.status == MUSE_ON_STATUS_SAFETY_LATCH) {
    if (_coordinator.disable_pending) {
      return @"Control: Inactive — Safety latch — Disable Pending";
    }
    return [NSString stringWithFormat:@"Control: Inactive — Safety latch — %s",
                                      muse_on_safety_failure_string(
                                          _coordinator.safety_failure)];
  }
  return [NSString stringWithFormat:@"Control: Inactive — %s",
                                    muse_on_inactive_reason_string(
                                        _coordinator.inactive_reason)];
}

- (void)refreshStatusIcon {
  if (!self.statusItem.button) return;
  NSString *symbolName;
  NSString *fallbackTitle;
  NSString *stateLabel;

  switch (_coordinator.status) {
    case MUSE_ON_STATUS_ACTIVE:
      symbolName = @"checkmark.circle";
      fallbackTitle = @"✓";
      stateLabel = @"Active — dispatch permitted";
      break;
    case MUSE_ON_STATUS_INACTIVE:
      symbolName = @"circle.dashed";
      fallbackTitle = @"·";
      stateLabel = @"Inactive — dispatch blocked";
      break;
    case MUSE_ON_STATUS_SAFETY_LATCH:
      symbolName = @"exclamationmark.triangle";
      fallbackTitle = @"!";
      stateLabel = @"Safety latch — Retry required";
      break;
    case MUSE_ON_STATUS_DISABLED:
      symbolName = @"minus.circle";
      fallbackTitle = @"−";
      stateLabel = @"Disabled — control off";
      break;
  }

  NSImage *image = nil;
  if (@available(macOS 11.0, *)) {
    image = [NSImage imageWithSystemSymbolName:symbolName
                         accessibilityDescription:stateLabel];
  }
  if (image) {
    image.template = YES;
    self.statusItem.button.image = image;
    self.statusItem.button.title = @"";
  } else {
    self.statusItem.button.image = nil;
    self.statusItem.button.title = fallbackTitle;
  }
  self.statusItem.button.toolTip = [NSString stringWithFormat:
      @"Codex Muse-On — %@", stateLabel];
  self.statusItem.button.accessibilityLabel = [NSString stringWithFormat:
      @"Codex Muse-On, %@", stateLabel];
}

- (void)refreshMenuWithCommand:(MuseOnCommand)command {
  [self updateCoordinatorWithCommand:command];
  [self refreshStatusIcon];
  NSStackView *stack = [NSStackView stackViewWithViews:@[]];
  stack.orientation = NSUserInterfaceLayoutOrientationVertical;
  stack.alignment = NSLayoutAttributeLeading;
  stack.spacing = 10;
  stack.translatesAutoresizingMaskIntoConstraints = NO;

  NSTextField *(^label)(NSString *) = ^NSTextField *(NSString *text) {
    NSTextField *field = [NSTextField labelWithString:text];
    field.lineBreakMode = NSLineBreakByTruncatingTail;
    field.accessibilityLabel = text;
    return field;
  };
  [stack addArrangedSubview:label(self.controllerConnected
      ? @"Muse-On: Connected" : @"Muse-On: Disconnected")];
  [stack addArrangedSubview:label([self controlStatusText])];

  NSString *controlTitle = _coordinator.disable_pending
      ? @"Disable Pending"
      : (_setup.enabled ? @"Disable Codex Muse-On" : @"Enable Codex Muse-On…");
  NSButton *controlButton = [NSButton buttonWithTitle:controlTitle
                                                target:self
                                                action:(_setup.enabled ? @selector(disable:)
                                                                      : @selector(enable:))];
  controlButton.enabled = !_coordinator.disable_pending;
  controlButton.accessibilityLabel = controlTitle;
  [stack addArrangedSubview:controlButton];

  NSStackView *profileRow = [NSStackView stackViewWithViews:@[]];
  profileRow.orientation = NSUserInterfaceLayoutOrientationHorizontal;
  profileRow.alignment = NSLayoutAttributeCenterY;
  profileRow.spacing = 8;
  NSTextField *profileLabel = label(@"Control Profile");
  [profileRow addArrangedSubview:profileLabel];
  NSPopUpButton *profile = [[NSPopUpButton alloc] initWithFrame:NSZeroRect
                                                      pullsDown:NO];
  [profile addItemsWithTitles:@[@"Controller Only", @"Pedal Enabled"]];
  [profile selectItemAtIndex:_profile == MUSE_ON_PROFILE_PEDAL ? 1 : 0];
  profile.target = self;
  profile.action = @selector(changeProfile:);
  profile.accessibilityLabel = @"Control Profile";
  profile.accessibilityRole = NSAccessibilityPopUpButtonRole;
  [profileRow addArrangedSubview:profile];
  [stack addArrangedSubview:profileRow];

  NSTextField *mapTitle = label([NSString stringWithFormat:
      @"Controller Map — %@ (read-only)", MuseOnProfileTitle(_profile)]);
  mapTitle.font = [NSFont systemFontOfSize:13 weight:NSFontWeightSemibold];
  [stack addArrangedSubview:mapTitle];

  MuseOnControllerMapView *mapView =
      [[MuseOnControllerMapView alloc] initWithProfile:_profile];
  mapView.frame = NSMakeRect(0, 0, 728, 620);
  mapView.autoresizingMask = NSViewWidthSizable;
  NSScrollView *mapScroll = [[NSScrollView alloc] initWithFrame:NSZeroRect];
  mapScroll.hasVerticalScroller = YES;
  mapScroll.hasHorizontalScroller = NO;
  mapScroll.autohidesScrollers = YES;
  mapScroll.drawsBackground = NO;
  mapScroll.borderType = NSNoBorder;
  mapScroll.accessibilityLabel = @"Controller Map text equivalent";
  mapScroll.documentView = mapView;
  mapScroll.translatesAutoresizingMaskIntoConstraints = NO;
  [mapScroll.widthAnchor constraintEqualToConstant:728].active = YES;
  [mapScroll.heightAnchor constraintEqualToConstant:390].active = YES;
  [stack addArrangedSubview:mapScroll];

  NSButton *startup = [NSButton checkboxWithTitle:@"Start Automatically"
                                            target:self action:@selector(toggleStartup:)];
  startup.state = _setup.start_automatically ? NSControlStateValueOn
                                              : NSControlStateValueOff;
  startup.accessibilityLabel = @"Start Automatically";
  startup.accessibilityRole = NSAccessibilityCheckBoxRole;
  [stack addArrangedSubview:startup];

  NSButton *(^actionButton)(NSString *, SEL) = ^NSButton *(NSString *title,
                                                            SEL selector) {
    NSButton *button = [NSButton buttonWithTitle:title target:self action:selector];
    button.accessibilityLabel = title;
    return button;
  };
  NSMutableArray<NSView *> *focusable = [NSMutableArray arrayWithObjects:
      controlButton, profile, startup, nil];
  BOOL permissionNeedsSettings =
      _coordinator.status == MUSE_ON_STATUS_INACTIVE &&
      _coordinator.inactive_reason == MUSE_ON_INACTIVE_REASON_PERMISSION;
  BOOL startupNeedsSettings = [[NSUserDefaults standardUserDefaults]
                                  boolForKey:kStartupApprovalRequiredKey];
  BOOL retryNeeded = permissionNeedsSettings || startupNeedsSettings ||
                     _coordinator.safety_latched || _coordinator.disable_pending;
  if (startupNeedsSettings) {
    [stack addArrangedSubview:label(@"Start Automatically — Approval Required")];
  }
  if (permissionNeedsSettings) {
    NSButton *settings = actionButton(@"Open Settings…", @selector(openSettings:));
    [stack addArrangedSubview:settings];
    [focusable addObject:settings];
  }
  if (startupNeedsSettings) {
    NSButton *loginItems = actionButton(@"Open Login Items", @selector(openLoginItems:));
    [stack addArrangedSubview:loginItems];
    [focusable addObject:loginItems];
  }
  if (retryNeeded) {
    NSButton *retry = actionButton(@"Retry", @selector(retry:));
    [stack addArrangedSubview:retry];
    [focusable addObject:retry];
  }
  NSButton *copy = actionButton(@"Copy Diagnostics", @selector(copyDiagnostics:));
  NSButton *report = actionButton(@"Report a Problem…", @selector(reportProblem:));
  NSButton *quit = actionButton(@"Quit Codex Muse-On", @selector(quit:));
  [stack addArrangedSubview:copy];
  [stack addArrangedSubview:report];
  [stack addArrangedSubview:quit];
  [focusable addObject:copy];
  [focusable addObject:report];
  [focusable addObject:quit];
  for (NSUInteger index = 0; index < focusable.count; index++) {
    focusable[index].nextKeyView = focusable[(index + 1) % focusable.count];
  }

  NSView *view = [[NSView alloc] initWithFrame:NSMakeRect(0, 0, 760, 760)];
  view.accessibilityRole = NSAccessibilityGroupRole;
  view.accessibilityLabel = @"Codex Muse-On status popover";
  [view addSubview:stack];
  [NSLayoutConstraint activateConstraints:@[
      [stack.leadingAnchor constraintEqualToAnchor:view.leadingAnchor constant:16],
      [stack.trailingAnchor constraintEqualToAnchor:view.trailingAnchor constant:-16],
      [stack.topAnchor constraintEqualToAnchor:view.topAnchor constant:16],
      [stack.bottomAnchor constraintLessThanOrEqualToAnchor:view.bottomAnchor constant:-16],
  ]];
  NSViewController *controller = [[NSViewController alloc] init];
  controller.view = view;
  self.popover.contentViewController = controller;
  self.popover.contentSize = NSMakeSize(760, 760);
}

- (void)refreshMenu {
  [self refreshMenuWithCommand:MUSE_ON_COMMAND_NONE];
}

- (void)notifySafetyLatchIfAuthorized {
  if (@available(macOS 10.14, *)) {
    MuseOnSafetyFailure failure = _coordinator.safety_failure;
    [[UNUserNotificationCenter currentNotificationCenter]
        getNotificationSettingsWithCompletionHandler:
            ^(UNNotificationSettings *settings) {
              if (settings.authorizationStatus !=
                  UNAuthorizationStatusAuthorized) return;
              UNMutableNotificationContent *content =
                  [[UNMutableNotificationContent alloc] init];
              content.title = @"Codex Muse-On safety latch";
              content.body = [NSString stringWithFormat:
                  @"Control is blocked: %s. Open the menu and choose Retry.",
                  muse_on_safety_failure_string(failure)];
              UNNotificationRequest *request = [UNNotificationRequest
                  requestWithIdentifier:@"codex-muse-on-safety-latch"
                  content:content trigger:nil];
              [[UNUserNotificationCenter currentNotificationCenter]
                  addNotificationRequest:request withCompletionHandler:nil];
            }];
  }
}

- (void)finishSafeQuit {
  [[NSUserDefaults standardUserDefaults] setBool:YES
                                           forKey:kSafeQuitVerifiedKey];
  muse_on_diagnostics_clear(&_diagnostics);
  self.quitInFlight = NO;
}

- (void)applyStartupPreference {
  if (@available(macOS 13.0, *)) {
    SMAppService *service = [SMAppService mainAppService];
    NSError *error = nil;
    BOOL succeeded = _setup.start_automatically
                         ? [service registerAndReturnError:&error]
                         : [service unregisterAndReturnError:&error];
    [[NSUserDefaults standardUserDefaults] setBool:!succeeded
                                            forKey:kStartupApprovalRequiredKey];
  }
}

- (void)enable:(id)sender {
  if (_coordinator.disable_pending) return;
  NSAlert *alert = [[NSAlert alloc] init];
  alert.messageText = @"Enable Codex Muse-On?";
  alert.informativeText =
      @"Codex Muse-On only sends controls when its safety checks pass. "
       "macOS may ask for permissions now; startup will be enabled by default.";
  [alert addButtonWithTitle:@"Enable"];
  [alert addButtonWithTitle:@"Cancel"];
  if ([alert runModal] != NSAlertFirstButtonReturn) return;

  NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
  BOOL isFirstEnable = ![defaults boolForKey:kFirstEnableCompletedKey];
  muse_on_setup_state_confirm_first_enable(&_setup);
  [self saveSetup];
  if (isFirstEnable) {
    [defaults setBool:YES forKey:kFirstEnableCompletedKey];
    if (!muse_on_preflight_post_event_access()) (void)muse_on_request_post_event_access();
    if (@available(macOS 10.14, *)) {
      [[UNUserNotificationCenter currentNotificationCenter]
          requestAuthorizationWithOptions:UNAuthorizationOptionAlert
                         completionHandler:^(__unused BOOL granted,
                                             __unused NSError *error) {}];
    }
  }
  [self applyStartupPreference];
  [self startListenerIfNeeded];
  [self refreshMenuWithCommand:MUSE_ON_COMMAND_ENABLE];
}

- (void)disable:(id)sender {
  BOOL cleanupVerified;

  muse_on_setup_state_disable(&_setup);
  [self saveSetup];
  if (self.listenerTask) {
    self.listenerStopPurpose = kListenerStopForDisable;
    [self applyCoordinatorCommand:MUSE_ON_COMMAND_DISABLE
                 cleanupVerified:NO
                   safetyFailure:self.listenerSafetyFailure];
    [self.listenerTask terminate];
  } else {
    cleanupVerified = !_coordinator.safety_latched &&
                      !_coordinator.disable_pending &&
                      (!self.listenerEverStarted || self.listenerCleanupKnown);
    if (cleanupVerified) {
      self.listenerSafetyFailure = MUSE_ON_SAFETY_FAILURE_NONE;
    }
    [self applyCoordinatorCommand:MUSE_ON_COMMAND_DISABLE
                 cleanupVerified:cleanupVerified
                   safetyFailure:cleanupVerified
                       ? MUSE_ON_SAFETY_FAILURE_NONE
                       : (self.listenerSafetyFailure != MUSE_ON_SAFETY_FAILURE_NONE
                              ? self.listenerSafetyFailure
                              : MUSE_ON_SAFETY_FAILURE_DEVICE_UNCERTAIN)];
  }
  [self refreshMenu];
}

- (void)changeProfile:(NSPopUpButton *)sender {
  MuseOnProfile next = sender.indexOfSelectedItem == 1
      ? MUSE_ON_PROFILE_PEDAL : MUSE_ON_PROFILE_CONTROLLER_ONLY;
  if (_profile == next) return;
  /* SIGTERM invokes the listener's safe release/restoration path first. */
  _profile = next;
  [[NSUserDefaults standardUserDefaults] setObject:
      (_profile == MUSE_ON_PROFILE_PEDAL ? @"pedal" : @"controller-only")
                                         forKey:kControlProfileKey];
  if (self.listenerTask) {
    self.listenerStopPurpose = kListenerStopForProfile;
    [self.listenerTask terminate];
  } else {
    [self startListenerIfNeeded];
  }
  [self refreshMenu];
}

- (void)toggleStartup:(id)sender {
  muse_on_setup_state_set_start_automatically(&_setup, !_setup.start_automatically);
  [self saveSetup];
  [self applyStartupPreference];
  [self refreshMenu];
}

- (void)openSettings:(id)sender {
  NSURL *url = [NSURL URLWithString:@"x-apple.systempreferences:com.apple.preference.security"];
  [[NSWorkspace sharedWorkspace] openURL:url];
}

- (void)openLoginItems:(id)sender {
  NSURL *url = [NSURL URLWithString:
      @"x-apple.systempreferences:com.apple.LoginItems-Settings.extension"];
  [[NSWorkspace sharedWorkspace] openURL:url];
}

- (void)copyDiagnostics:(id)sender {
  char output[4096];
  size_t written = muse_on_diagnostics_copy(&_diagnostics, output,
                                            sizeof(output));
  NSString *text = [[NSString alloc] initWithBytes:output
                                            length:written
                                          encoding:NSUTF8StringEncoding];
  if (!text) text = @"No diagnostics recorded.";
  [[NSPasteboard generalPasteboard] clearContents];
  [[NSPasteboard generalPasteboard] setString:text forType:NSPasteboardTypeString];
}

- (void)reportProblem:(id)sender {
  NSURL *url = [NSURL URLWithString:
      @"https://github.com/Kokoabassplayer/codex-muse-on/issues/new?title=Codex%20Muse-On%20problem"];
  [[NSWorkspace sharedWorkspace] openURL:url];
}

- (void)retry:(id)sender {
  if (_coordinator.disable_pending) {
    self.listenerStopPurpose = kListenerStopForDisable;
  } else if (_coordinator.safety_latched) {
    self.listenerStopPurpose = kListenerStopForRetry;
  } else {
    [self refreshMenuWithCommand:MUSE_ON_COMMAND_RETRY];
    return;
  }
  if (self.listenerTask) {
    [self.listenerTask terminate];
  } else {
    [self startListenerWithSafetyLatch:YES];
  }
  [self refreshMenu];
}

- (void)quit:(id)sender { [NSApp terminate:nil]; }

- (void)showPopover:(id)sender {
  if (self.popover.shown) {
    [self.popover performClose:sender];
    return;
  }
  [self refreshMenu];
  [self.popover showRelativeToRect:self.statusItem.button.bounds
                            ofView:self.statusItem.button
                            preferredEdge:NSRectEdgeMinY];
}

- (NSApplicationTerminateReply)applicationShouldTerminate:(NSApplication *)sender {
  if (!self.primaryInstance) return NSTerminateNow;
  if (self.quitInFlight) return NSTerminateLater;
  if (_coordinator.safety_latched || _coordinator.disable_pending) {
    return NSTerminateCancel;
  }

  self.quitInFlight = YES;
  if (self.listenerTask) {
    self.listenerStopPurpose = kListenerStopForQuit;
    [self applyCoordinatorCommand:MUSE_ON_COMMAND_QUIT
                 cleanupVerified:NO
                   safetyFailure:self.listenerSafetyFailure];
    [self.listenerTask terminate];
    return NSTerminateLater;
  }

  if (self.listenerEverStarted && !self.listenerCleanupKnown) {
    self.quitInFlight = NO;
    return NSTerminateCancel;
  }

  [self applyCoordinatorCommand:MUSE_ON_COMMAND_QUIT
               cleanupVerified:YES
                 safetyFailure:MUSE_ON_SAFETY_FAILURE_NONE];
  if (_coordinator.quit_allowed) {
    [self finishSafeQuit];
    return NSTerminateNow;
  }
  self.quitInFlight = NO;
  return NSTerminateCancel;
}

- (void)applicationDidFinishLaunching:(NSNotification *)notification {
  if (![self acquirePrimaryInstanceLock]) {
    self.primaryInstance = NO;
    [NSApp terminate:nil];
    return;
  }
  self.primaryInstance = YES;
  [self loadSetup];
  [[[NSWorkspace sharedWorkspace] notificationCenter]
      addObserver:self
         selector:@selector(sessionDidChange:)
             name:NSWorkspaceSessionDidBecomeActiveNotification
           object:nil];
  [[[NSWorkspace sharedWorkspace] notificationCenter]
      addObserver:self
         selector:@selector(sessionDidChange:)
             name:NSWorkspaceSessionDidResignActiveNotification
           object:nil];
  if (_setup.enabled) [self updateCoordinatorWithCommand:MUSE_ON_COMMAND_ENABLE];
  else if ([[NSUserDefaults standardUserDefaults]
                boolForKey:kDisablePendingKey]) {
    [self applyCoordinatorCommand:MUSE_ON_COMMAND_DISABLE
                 cleanupVerified:NO
                   safetyFailure:self.listenerSafetyFailure];
  }
  self.statusItem = [[NSStatusBar systemStatusBar] statusItemWithLength:NSVariableStatusItemLength];
  self.statusItem.button.title = @"◎";
  self.statusItem.button.accessibilityLabel = @"Codex Muse-On";
  self.statusItem.button.target = self;
  self.statusItem.button.action = @selector(showPopover:);
  self.popover = [[NSPopover alloc] init];
  self.popover.behavior = NSPopoverBehaviorTransient;
  [self refreshMenu];
  if (_setup.enabled && !_coordinator.safety_latched) {
    [self startListenerIfNeeded];
  }
}

- (void)applicationWillTerminate:(NSNotification *)notification {
  if (!self.primaryInstance) return;
  [[[NSWorkspace sharedWorkspace] notificationCenter] removeObserver:self];
  muse_on_diagnostics_clear(&_diagnostics);
  if (self.lockFd >= 0) close(self.lockFd);
}

@end

int main(int argc, const char *argv[]) {
  (void)argc;
  (void)argv;
  @autoreleasepool {
    NSApplication *application = [NSApplication sharedApplication];
    MuseOnAppDelegate *delegate = [[MuseOnAppDelegate alloc] init];
    delegate.lockFd = -1;
    application.delegate = delegate;
    [application run];
  }
  return 0;
}
