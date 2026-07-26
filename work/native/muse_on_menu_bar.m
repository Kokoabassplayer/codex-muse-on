#import <AppKit/AppKit.h>
#import <ServiceManagement/ServiceManagement.h>
#import <UserNotifications/UserNotifications.h>

#include <fcntl.h>
#include <sys/file.h>
#include <unistd.h>

#include "muse_on_platform.h"
#include "muse_on_setup_state.h"
#include "muse_on_state_coordinator.h"

static NSString *const kEnabledIntentKey = @"enabledIntent";
static NSString *const kFirstEnableCompletedKey = @"firstEnableCompleted";
static NSString *const kStartAutomaticallyKey = @"startAutomatically";
static NSString *const kStartupApprovalRequiredKey = @"startupApprovalRequired";

@interface MuseOnAppDelegate : NSObject <NSApplicationDelegate>
@property(nonatomic, strong) NSStatusItem *statusItem;
@property(nonatomic, strong) NSPopover *popover;
@property(nonatomic) MuseOnSetupState setup;
@property(nonatomic) MuseOnState coordinator;
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
}

- (void)saveSetup {
  NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
  [defaults setBool:_setup.enabled forKey:kEnabledIntentKey];
  [defaults setBool:_setup.start_automatically forKey:kStartAutomaticallyKey];
}

- (void)updateCoordinatorWithCommand:(MuseOnCommand)command {
  MuseOnPrerequisites prerequisites = {
      .safety_latched = false,
      .permission_granted = muse_on_preflight_post_event_access(),
      .controller_connected = false, /* HID adapter lands in a later ticket. */
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
  [stack addArrangedSubview:label(@"Muse-On: Disconnected")];
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
  [self refreshMenuWithCommand:MUSE_ON_COMMAND_ENABLE];
}

- (void)disable:(id)sender {
  muse_on_setup_state_disable(&_setup);
  [self saveSetup];
  [self refreshMenuWithCommand:MUSE_ON_COMMAND_DISABLE];
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
}

- (void)applicationWillTerminate:(NSNotification *)notification {
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
