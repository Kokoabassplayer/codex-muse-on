#import <AppKit/AppKit.h>
#import <ServiceManagement/ServiceManagement.h>
#import <UserNotifications/UserNotifications.h>

#include <fcntl.h>
#include <sys/file.h>
#include <unistd.h>

#include "muse_on_platform.h"
#include "muse_on_action_map.h"
#include "muse_on_connection.h"
#include "muse_on_setup_state.h"
#include "muse_on_state_coordinator.h"

static NSString *const kEnabledIntentKey = @"enabledIntent";
static NSString *const kFirstEnableCompletedKey = @"firstEnableCompleted";
static NSString *const kStartAutomaticallyKey = @"startAutomatically";
static NSString *const kControlProfileKey = @"controlProfile";
static NSString *const kStartupApprovalRequiredKey = @"startupApprovalRequired";

@interface MuseOnAppDelegate : NSObject <NSApplicationDelegate>
@property(nonatomic, strong) NSStatusItem *statusItem;
@property(nonatomic, strong) NSPopover *popover;
@property(nonatomic, strong) NSTask *listenerTask;
@property(nonatomic, strong) NSMutableDictionary<NSString *, NSDictionary *> *interfaces;
@property(nonatomic, strong) NSMutableData *listenerOutputBuffer;
@property(nonatomic) BOOL restartListenerAfterExit;
@property(nonatomic) BOOL controllerConnected;
@property(nonatomic) MuseOnSetupState setup;
@property(nonatomic) MuseOnState coordinator;
@property(nonatomic) MuseOnProfile profile;
@property(nonatomic) int lockFd;
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
  muse_on_setup_state_init(&_setup, [defaults boolForKey:kEnabledIntentKey],
                          [defaults boolForKey:kStartAutomaticallyKey]);
  muse_on_state_init(&_coordinator);
  _profile = [[defaults stringForKey:kControlProfileKey] isEqualToString:@"pedal"]
      ? MUSE_ON_PROFILE_PEDAL : MUSE_ON_PROFILE_CONTROLLER_ONLY;
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
        [entry[@"kind"] integerValue], [entry[@"location"] unsignedIntValue], true};
  }
  connected = muse_on_find_single_complete_controller(interfaces, count, &locationID);
  free(interfaces);
  return connected;
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
      self.controllerConnected = [self hasSingleCompleteController];
      if (self.popover.shown) [self refreshMenu];
    }
  }
}

- (void)stopListener {
  self.restartListenerAfterExit = NO;
  if (self.listenerTask.running) [self.listenerTask terminate];
}

- (void)startListenerIfNeeded {
  NSString *path;
  NSTask *task;
  if (!_setup.enabled || self.listenerTask != nil) return;
  path = [[NSBundle mainBundle] pathForAuxiliaryExecutable:@"muse_on_listener"];
  if (!path) return;
  task = [[NSTask alloc] init];
  task.launchPath = path;
  task.arguments = @[@"--mode=active", _profile == MUSE_ON_PROFILE_PEDAL
      ? @"--profile=pedal" : @"--profile=controller-only"];
  self.interfaces = [NSMutableDictionary dictionary];
  self.listenerOutputBuffer = [NSMutableData data];
  self.controllerConnected = NO;
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
  task.terminationHandler = ^(NSTask *finishedTask) {
    dispatch_async(dispatch_get_main_queue(), ^{
      if (weakSelf.listenerTask != finishedTask) return;
      weakSelf.listenerTask = nil;
      weakSelf.controllerConnected = NO;
      [weakSelf.interfaces removeAllObjects];
      if (weakSelf.restartListenerAfterExit) {
        weakSelf.restartListenerAfterExit = NO;
        [weakSelf startListenerIfNeeded];
      }
    });
  };
  @try {
    [task launch];
    self.listenerTask = task;
  } @catch (__unused NSException *exception) {
    self.listenerTask = nil;
  }
}

- (void)updateCoordinatorWithCommand:(MuseOnCommand)command {
  MuseOnPrerequisites prerequisites = {
      .safety_latched = false,
      .permission_granted = muse_on_preflight_post_event_access(),
      .controller_connected = self.controllerConnected,
      .multiple_controllers = false,
      .session_available = true,
      .codex_foreground = muse_on_codex_is_frontmost(),
      .inputs_released = true,
  };
  muse_on_state_apply(&_coordinator, command, prerequisites);
}

- (NSString *)controlStatusText {
  if (_coordinator.status == MUSE_ON_STATUS_DISABLED) return @"Control: Disabled";
  if (_coordinator.status == MUSE_ON_STATUS_ACTIVE) return @"Control: Active";
  if (_coordinator.status == MUSE_ON_STATUS_SAFETY_LATCH) {
    return @"Control: Inactive — Safety latch";
  }
  return [NSString stringWithFormat:@"Control: Inactive — %s",
                                    muse_on_inactive_reason_string(
                                        _coordinator.inactive_reason)];
}

- (void)refreshMenuWithCommand:(MuseOnCommand)command {
  [self updateCoordinatorWithCommand:command];
  NSStackView *stack = [NSStackView stackViewWithViews:@[]];
  stack.orientation = NSUserInterfaceLayoutOrientationVertical;
  stack.alignment = NSLayoutAttributeLeading;
  stack.spacing = 8;
  stack.translatesAutoresizingMaskIntoConstraints = NO;

  NSTextField *(^label)(NSString *) = ^NSTextField *(NSString *text) {
    NSTextField *field = [NSTextField labelWithString:text];
    field.lineBreakMode = NSLineBreakByTruncatingTail;
    return field;
  };
  [stack addArrangedSubview:label(self.controllerConnected
      ? @"Muse-On: Connected" : @"Muse-On: Disconnected")];
  [stack addArrangedSubview:label([self controlStatusText])];

  NSButton *controlButton = [NSButton buttonWithTitle:
      (_setup.enabled ? @"Disable Codex Muse-On" : @"Enable Codex Muse-On…")
                                                target:self
                                                action:(_setup.enabled ? @selector(disable:)
                                                                      : @selector(enable:))];
  [stack addArrangedSubview:controlButton];

  NSButton *startup = [NSButton checkboxWithTitle:@"Start Automatically"
                                            target:self action:@selector(toggleStartup:)];
  startup.state = _setup.start_automatically ? NSControlStateValueOn
                                              : NSControlStateValueOff;
  [stack addArrangedSubview:startup];
  NSPopUpButton *profile = [[NSPopUpButton alloc] initWithFrame:NSZeroRect pullsDown:NO];
  [profile addItemsWithTitles:@[@"Controller Only", @"Pedal Enabled"]];
  [profile selectItemAtIndex:_profile == MUSE_ON_PROFILE_PEDAL ? 1 : 0];
  profile.target = self;
  profile.action = @selector(changeProfile:);
  [stack addArrangedSubview:profile];
  if ([[NSUserDefaults standardUserDefaults] boolForKey:kStartupApprovalRequiredKey]) {
    [stack addArrangedSubview:label(@"Start Automatically — Approval Required")];
  }
  [stack addArrangedSubview:[NSButton buttonWithTitle:@"Open Settings…" target:self
                                                action:@selector(openSettings:)]];
  [stack addArrangedSubview:[NSButton buttonWithTitle:@"Retry" target:self
                                                action:@selector(retry:)]];
  [stack addArrangedSubview:[NSButton buttonWithTitle:@"Quit Codex Muse-On" target:self
                                                action:@selector(quit:)]];

  NSView *view = [[NSView alloc] initWithFrame:NSMakeRect(0, 0, 330, 220)];
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
}

- (void)refreshMenu {
  [self refreshMenuWithCommand:MUSE_ON_COMMAND_NONE];
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
  [self stopListener];
  muse_on_setup_state_disable(&_setup);
  [self saveSetup];
  [self refreshMenuWithCommand:MUSE_ON_COMMAND_DISABLE];
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
    self.restartListenerAfterExit = YES;
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

- (void)retry:(id)sender { [self refreshMenuWithCommand:MUSE_ON_COMMAND_RETRY]; }
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

- (void)applicationDidFinishLaunching:(NSNotification *)notification {
  if (![self acquirePrimaryInstanceLock]) {
    [NSApp terminate:nil];
    return;
  }
  [self loadSetup];
  if (_setup.enabled) [self updateCoordinatorWithCommand:MUSE_ON_COMMAND_ENABLE];
  self.statusItem = [[NSStatusBar systemStatusBar] statusItemWithLength:NSVariableStatusItemLength];
  self.statusItem.button.title = @"◎";
  self.statusItem.button.accessibilityLabel = @"Codex Muse-On";
  self.statusItem.button.target = self;
  self.statusItem.button.action = @selector(showPopover:);
  self.popover = [[NSPopover alloc] init];
  self.popover.behavior = NSPopoverBehaviorTransient;
  [self refreshMenu];
  [self startListenerIfNeeded];
}

- (void)applicationWillTerminate:(NSNotification *)notification {
  [self stopListener];
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
