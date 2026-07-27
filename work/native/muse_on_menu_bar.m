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
#include "muse_on_connection_observer.h"
#include "muse_on_diagnostics.h"
#include "muse_on_quit_policy.h"
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

static NSString *MuseOnStringFromUTF8(const char *value) {
  NSString *string = value ? [NSString stringWithUTF8String:value] : nil;
  if (string) return string;
  return @"Unknown control";
}

static NSString *MuseOnDefaultControlIdentifier(void) {
  for (size_t index = 0; index < muse_on_control_mapping_count(); index++) {
    const MuseOnControlMapping *mapping = muse_on_control_mapping_at(index);
    if (strcmp(mapping->identifier, "black2") == 0) {
      return MuseOnStringFromUTF8(mapping->identifier);
    }
  }
  return MuseOnStringFromUTF8(
      muse_on_control_mapping_at(0)->identifier);
}

static NSString *MuseOnControlDescription(const MuseOnControlMapping *mapping,
                                           MuseOnProfile profile);

static NSString *MuseOnControlText(const MuseOnControlMapping *mapping,
                                   MuseOnProfile profile) {
  const MuseOnControlProfileMapping *profile_mapping =
      muse_on_control_mapping_profile(mapping, profile);
  NSString *physical = MuseOnStringFromUTF8(mapping->physical_label);

  if (!profile_mapping || !profile_mapping->available) {
    return [NSString stringWithFormat:
        @"%@ → Not active in %@; select Pedal Enabled to use it.", physical,
        MuseOnProfileTitle(profile)];
  }
  return [NSString stringWithFormat:@"%@ → %s. %@", physical,
                                    muse_on_action_display_name(
                                        profile_mapping->press_action),
                                    MuseOnControlDescription(mapping, profile)];
}

static NSString *MuseOnControlDescription(const MuseOnControlMapping *mapping,
                                           MuseOnProfile profile) {
  const MuseOnControlProfileMapping *profile_mapping =
      muse_on_control_mapping_profile(mapping, profile);

  if (!profile_mapping || !profile_mapping->available) {
    return @"Available when Pedal Enabled is selected.";
  }
  NSString *identifier = MuseOnStringFromUTF8(mapping->identifier);
  NSString *interaction = MuseOnStringFromUTF8(
      muse_on_action_phase_prompt(profile_mapping->press_phase));
  if ([identifier isEqualToString:@"black8"]) {
    NSString *target = profile == MUSE_ON_PROFILE_PEDAL
        ? @"the environment action" : @"Global Dictation";
    return [NSString stringWithFormat:@"%@ %@.", interaction, target];
  }
  if ([identifier isEqualToString:@"pedal"]) {
    return [NSString stringWithFormat:@"%@ Push-to-Talk.", interaction];
  }
  NSDictionary<NSString *, NSString *> *descriptions = @{
      @"white1" : @"Switch to Fast Mode.",
      @"white3" : @"Decline the current suggestion.",
      @"white5" : @"Copy the conversation as Markdown.",
      @"white7" : @"Open the review tab.",
      @"black2" : @"Approve the current suggestion.",
      @"black4" : @"Fork the current thread.",
      @"black6" : @"Send the current composer message.",
      @"leftBall.north" : @"Cycle to the previous thread.",
      @"leftBall.south" : @"Cycle to the next thread.",
      @"rightBall.vertical" : @"Open the model picker.",
      @"rightBall.west" : @"Navigate back in history.",
      @"rightBall.east" : @"Navigate forward in history.",
      @"turntable.clockwise" : @"Decrease reasoning effort.",
      @"turntable.counterclockwise" : @"Increase reasoning effort.",
  };
  NSString *description = descriptions[identifier];
  if (description) return description;
  return [NSString stringWithFormat:@"%@ this Codex action.", interaction];
}

static NSRect MuseOnNormalizedRect(const MuseOnControlMapping *mapping,
                                   NSSize size) {
  return NSMakeRect(mapping->x * size.width, mapping->y * size.height,
                    mapping->width * size.width,
                    mapping->height * size.height);
}

@interface MuseOnControllerMapCanvas : NSView
@property(nonatomic) MuseOnProfile profile;
@property(nonatomic, copy) NSString *selectedIdentifier;
@property(nonatomic, copy) void (^selectionHandler)(NSString *identifier);
- (instancetype)initWithProfile:(MuseOnProfile)profile;
@end

@implementation MuseOnControllerMapCanvas

- (instancetype)initWithProfile:(MuseOnProfile)profile {
  self = [super initWithFrame:NSZeroRect];
  if (self) {
    _profile = profile;
    self.wantsLayer = YES;
    self.accessibilityRole = NSAccessibilityImageRole;
    self.accessibilityLabel = @"Muse-On Control Map";
    self.accessibilityValue = [NSString stringWithFormat:
        @"Selected profile: %@. Select a control below to inspect its action.",
        MuseOnProfileTitle(profile)];
  }
  return self;
}

- (BOOL)isFlipped {
  return YES;
}

- (NSSize)intrinsicContentSize {
  return NSMakeSize(464, 229);
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

- (void)drawCenteredLabel:(NSString *)label
                   inRect:(NSRect)rect
                      font:(NSFont *)font
                     color:(NSColor *)color {
  NSSize labelSize = [label sizeWithAttributes:@{NSFontAttributeName : font}];
  [self drawLabel:label
          atPoint:NSMakePoint(NSMidX(rect) - labelSize.width / 2.0,
                              NSMidY(rect) - labelSize.height / 2.0)
             font:font
            color:color];
}

- (BOOL)isSelectedMapping:(const MuseOnControlMapping *)mapping {
  return self.selectedIdentifier != nil &&
         [self.selectedIdentifier isEqualToString:
              MuseOnStringFromUTF8(mapping->identifier)];
}

- (void)drawSelectionRingForRect:(NSRect)rect {
  NSBezierPath *ring = [NSBezierPath bezierPathWithRoundedRect:
      NSInsetRect(rect, -3, -3) xRadius:8 yRadius:8];
  [[NSColor controlAccentColor] setStroke];
  ring.lineWidth = 2.4;
  [ring stroke];
}

- (void)drawSelectionRingForCircleInRect:(NSRect)rect {
  NSBezierPath *ring = [NSBezierPath bezierPathWithOvalInRect:
      NSInsetRect(rect, -3, -3)];
  [[NSColor controlAccentColor] setStroke];
  ring.lineWidth = 2.4;
  [ring stroke];
}

- (void)drawTriangleWithTip:(NSPoint)tip
                     baseOne:(NSPoint)baseOne
                     baseTwo:(NSPoint)baseTwo
                       color:(NSColor *)color {
  NSBezierPath *triangle = [NSBezierPath bezierPath];
  [triangle moveToPoint:tip];
  [triangle lineToPoint:baseOne];
  [triangle lineToPoint:baseTwo];
  [triangle closePath];
  [color setStroke];
  triangle.lineWidth = 1.2;
  [triangle stroke];
}

- (void)drawRect:(NSRect)dirtyRect {
  (void)dirtyRect;
  NSSize size = self.bounds.size;
  CGFloat scaleX = size.width / 450.0;
  CGFloat scaleY = size.height / 222.0;
  CGFloat scale = MIN(scaleX, scaleY);
  NSColor *diagramTop = [NSColor colorWithCalibratedWhite:0.18 alpha:1.0];
  NSColor *diagramBottom = [NSColor colorWithCalibratedWhite:0.12 alpha:1.0];
  NSColor *bodyTop = [NSColor colorWithCalibratedWhite:0.21 alpha:1.0];
  NSColor *bodyBottom = [NSColor colorWithCalibratedWhite:0.11 alpha:1.0];
  NSColor *stroke = [NSColor colorWithCalibratedWhite:0.36 alpha:1.0];
  NSColor *faint = [NSColor colorWithCalibratedWhite:0.43 alpha:1.0];
  NSColor *keyWhite = [NSColor colorWithCalibratedWhite:0.95 alpha:1.0];
  NSColor *keyWhiteLabel = [NSColor colorWithCalibratedWhite:0.40 alpha:1.0];
  NSColor *keyBlack = [NSColor colorWithCalibratedWhite:0.095 alpha:1.0];

  NSRect diagram = NSInsetRect(self.bounds, 1, 1);
  NSBezierPath *diagramPath = [NSBezierPath bezierPathWithRoundedRect:diagram
                                                                  xRadius:13
                                                                  yRadius:13];
  NSGradient *diagramGradient = [[NSGradient alloc]
      initWithStartingColor:diagramTop endingColor:diagramBottom];
  [diagramGradient drawInBezierPath:diagramPath angle:90];
  [stroke setStroke];
  diagramPath.lineWidth = 1.0;
  [diagramPath stroke];

  NSRect body = NSMakeRect(8 * scaleX, 8 * scaleY, 434 * scaleX, 152 * scaleY);
  NSBezierPath *bodyPath = [NSBezierPath bezierPathWithRoundedRect:body
                                                              xRadius:18 * scale
                                                              yRadius:18 * scale];
  NSGradient *bodyGradient = [[NSGradient alloc]
      initWithStartingColor:bodyTop endingColor:bodyBottom];
  [bodyGradient drawInBezierPath:bodyPath angle:90];
  [[NSColor colorWithCalibratedWhite:0.40 alpha:1.0] setStroke];
  bodyPath.lineWidth = 1.0;
  [bodyPath stroke];
  NSBezierPath *bodyHighlight = [NSBezierPath bezierPathWithRoundedRect:
      NSInsetRect(body, 1.5 * scale, 1.5 * scale)
      xRadius:16.5 * scale yRadius:16.5 * scale];
  [[NSColor colorWithCalibratedWhite:0.80 alpha:0.13] setStroke];
  bodyHighlight.lineWidth = 1.0;
  [bodyHighlight stroke];

  [self drawLabel:@"MUSE-ON"
          atPoint:NSMakePoint(24 * scaleX, 147 * scaleY)
              font:[NSFont systemFontOfSize:8 * scale weight:NSFontWeightSemibold]
             color:[NSColor colorWithCalibratedWhite:0.65 alpha:1.0]];
  [self drawLabel:@"BLACK KEYS"
          atPoint:NSMakePoint(217 * scaleX, 35 * scaleY)
              font:[NSFont systemFontOfSize:7 * scale weight:NSFontWeightSemibold]
             color:faint];
  [self drawLabel:@"WHITE KEYS"
          atPoint:NSMakePoint(194 * scaleX, 83 * scaleY)
              font:[NSFont systemFontOfSize:7 * scale weight:NSFontWeightSemibold]
             color:faint];

  NSPoint turntableCenter = NSMakePoint(84 * scaleX, 82 * scaleY);
  CGFloat turntableRadius = 54 * scale;
  NSRect turntable = NSMakeRect(turntableCenter.x - turntableRadius,
                                turntableCenter.y - turntableRadius,
                                turntableRadius * 2, turntableRadius * 2);
  NSBezierPath *turntableBase = [NSBezierPath bezierPathWithOvalInRect:turntable];
  [[NSColor colorWithCalibratedWhite:0.125 alpha:1.0] setFill];
  [turntableBase fill];
  [[NSColor colorWithCalibratedWhite:0.36 alpha:1.0] setStroke];
  turntableBase.lineWidth = 1.2 * scale;
  [turntableBase stroke];
  NSRect grooveRect = NSInsetRect(turntable, 7 * scale, 7 * scale);
  NSBezierPath *groove = [NSBezierPath bezierPathWithOvalInRect:grooveRect];
  CGFloat grooveDashes[] = {1.5 * scale, 3 * scale};
  [groove setLineDash:grooveDashes count:2 phase:0];
  [[NSColor colorWithCalibratedWhite:0.25 alpha:1.0] setStroke];
  groove.lineWidth = 1.8 * scale;
  [groove stroke];
  for (NSInteger ringInset = 16; ringInset <= 27; ringInset += 11) {
    NSBezierPath *ringPath = [NSBezierPath bezierPathWithOvalInRect:
        NSInsetRect(turntable, ringInset * scale, ringInset * scale)];
    [[NSColor colorWithCalibratedWhite:0.47 alpha:0.72] setStroke];
    ringPath.lineWidth = 1.0 * scale;
    [ringPath stroke];
  }
  NSBezierPath *hub = [NSBezierPath bezierPathWithOvalInRect:
      NSMakeRect(turntableCenter.x - 11 * scale, turntableCenter.y - 11 * scale,
                 22 * scale, 22 * scale)];
  [[NSColor colorWithCalibratedWhite:0.06 alpha:1.0] setFill];
  [hub fill];
  [[NSColor colorWithCalibratedWhite:0.46 alpha:1.0] setStroke];
  hub.lineWidth = 1.0 * scale;
  [hub stroke];
  [[NSColor controlAccentColor] setFill];
  NSBezierPath *turntableMark = [NSBezierPath bezierPathWithOvalInRect:
      NSMakeRect(turntableCenter.x + 27.5 * scale,
                 turntableCenter.y - 34.5 * scale, 7 * scale, 7 * scale)];
  [turntableMark fill];
  if ([self.selectedIdentifier hasPrefix:@"turntable."]) {
    [self drawSelectionRingForCircleInRect:turntable];
  }

  /* White keys are painted first; black keys sit above them in the map. */
  for (size_t index = 0; index < muse_on_control_mapping_count(); index++) {
    const MuseOnControlMapping *mapping = muse_on_control_mapping_at(index);
    if (mapping->group != MUSE_ON_CONTROL_GROUP_WHITE_BUTTONS) continue;
    NSRect frame = MuseOnNormalizedRect(mapping, size);
    NSBezierPath *key = [NSBezierPath bezierPathWithRoundedRect:frame
                                                           xRadius:5 * scale
                                                           yRadius:5 * scale];
    [keyWhite setFill];
    [key fill];
    [[NSColor colorWithCalibratedWhite:0.73 alpha:1.0] setStroke];
    key.lineWidth = 1.0 * scale;
    [key stroke];
    [self drawCenteredLabel:[NSString stringWithFormat:@"%c", mapping->identifier[5]]
                    inRect:frame
                       font:[NSFont systemFontOfSize:12 * scale
                                               weight:NSFontWeightSemibold]
                      color:keyWhiteLabel];
    if ([self isSelectedMapping:mapping]) [self drawSelectionRingForRect:frame];
  }

  for (size_t index = 0; index < muse_on_control_mapping_count(); index++) {
    const MuseOnControlMapping *mapping = muse_on_control_mapping_at(index);
    if (mapping->group != MUSE_ON_CONTROL_GROUP_BLACK_BUTTONS) continue;
    NSRect frame = MuseOnNormalizedRect(mapping, size);
    NSBezierPath *key = [NSBezierPath bezierPathWithRoundedRect:frame
                                                           xRadius:5 * scale
                                                           yRadius:5 * scale];
    [keyBlack setFill];
    [key fill];
    [[NSColor colorWithCalibratedWhite:0.29 alpha:1.0] setStroke];
    key.lineWidth = 1.0 * scale;
    [key stroke];
    [self drawCenteredLabel:[NSString stringWithFormat:@"%c", mapping->identifier[5]]
                    inRect:frame
                       font:[NSFont systemFontOfSize:12 * scale
                                               weight:NSFontWeightSemibold]
                      color:[NSColor colorWithCalibratedWhite:0.66 alpha:1.0]];
    if ([self isSelectedMapping:mapping]) [self drawSelectionRingForRect:frame];
  }

  NSPoint leftCenter = NSMakePoint(180 * scaleX, 58 * scaleY);
  NSPoint rightCenter = NSMakePoint(405 * scaleX, 107 * scaleY);
  CGFloat ballRadius = 15 * scale;
  NSRect leftBall = NSMakeRect(leftCenter.x - ballRadius, leftCenter.y - ballRadius,
                               ballRadius * 2, ballRadius * 2);
  NSRect rightBall = NSMakeRect(rightCenter.x - ballRadius, rightCenter.y - ballRadius,
                                ballRadius * 2, ballRadius * 2);
  for (NSValue *value in @[[NSValue valueWithRect:leftBall],
                           [NSValue valueWithRect:rightBall]]) {
    NSRect ball = value.rectValue;
    NSBezierPath *face = [NSBezierPath bezierPathWithOvalInRect:ball];
    [[NSColor colorWithCalibratedWhite:0.145 alpha:1.0] setFill];
    [face fill];
    [[NSColor colorWithCalibratedWhite:0.41 alpha:1.0] setStroke];
    face.lineWidth = 1.0 * scale;
    [face stroke];
    NSPoint center = NSMakePoint(NSMidX(ball), NSMidY(ball));
    NSBezierPath *core = [NSBezierPath bezierPathWithOvalInRect:
        NSMakeRect(center.x - 8.5 * scale, center.y - 8.5 * scale,
                   17 * scale, 17 * scale)];
    [[NSColor colorWithCalibratedWhite:0.067 alpha:1.0] setFill];
    [core fill];
    [[NSColor colorWithCalibratedWhite:0.52 alpha:1.0] setStroke];
    core.lineWidth = 0.8 * scale;
    [core stroke];
  }
  NSColor *activeGlyph = [NSColor colorWithCalibratedWhite:0.78 alpha:1.0];
  NSColor *inactiveGlyph = [NSColor colorWithCalibratedWhite:0.30 alpha:1.0];
  [self drawTriangleWithTip:NSMakePoint(leftCenter.x, leftCenter.y - 7 * scale)
                    baseOne:NSMakePoint(leftCenter.x - 2.5 * scale,
                                        leftCenter.y - 4 * scale)
                    baseTwo:NSMakePoint(leftCenter.x + 2.5 * scale,
                                        leftCenter.y - 4 * scale)
                      color:activeGlyph];
  [self drawTriangleWithTip:NSMakePoint(leftCenter.x, leftCenter.y + 7 * scale)
                    baseOne:NSMakePoint(leftCenter.x - 2.5 * scale,
                                        leftCenter.y + 4 * scale)
                    baseTwo:NSMakePoint(leftCenter.x + 2.5 * scale,
                                        leftCenter.y + 4 * scale)
                      color:activeGlyph];
  [self drawTriangleWithTip:NSMakePoint(leftCenter.x - 7 * scale, leftCenter.y)
                    baseOne:NSMakePoint(leftCenter.x - 4 * scale,
                                        leftCenter.y - 2.5 * scale)
                    baseTwo:NSMakePoint(leftCenter.x - 4 * scale,
                                        leftCenter.y + 2.5 * scale)
                      color:inactiveGlyph];
  [self drawTriangleWithTip:NSMakePoint(leftCenter.x + 7 * scale, leftCenter.y)
                    baseOne:NSMakePoint(leftCenter.x + 4 * scale,
                                        leftCenter.y - 2.5 * scale)
                    baseTwo:NSMakePoint(leftCenter.x + 4 * scale,
                                        leftCenter.y + 2.5 * scale)
                      color:inactiveGlyph];
  for (NSValue *value in @[
           [NSValue valueWithPoint:NSMakePoint(0, -1)],
           [NSValue valueWithPoint:NSMakePoint(0, 1)],
           [NSValue valueWithPoint:NSMakePoint(-1, 0)],
           [NSValue valueWithPoint:NSMakePoint(1, 0)]]) {
    NSPoint direction = value.pointValue;
    NSPoint tip = NSMakePoint(rightCenter.x + direction.x * 7 * scale,
                              rightCenter.y + direction.y * 7 * scale);
    NSPoint perpendicular = NSMakePoint(-direction.y * 2.5 * scale,
                                         direction.x * 2.5 * scale);
    [self drawTriangleWithTip:tip
                      baseOne:NSMakePoint(rightCenter.x + direction.x * 4 * scale
                                              + perpendicular.x,
                                          rightCenter.y + direction.y * 4 * scale
                                              + perpendicular.y)
                      baseTwo:NSMakePoint(rightCenter.x + direction.x * 4 * scale
                                              - perpendicular.x,
                                          rightCenter.y + direction.y * 4 * scale
                                              - perpendicular.y)
                        color:activeGlyph];
  }
  if ([self.selectedIdentifier hasPrefix:@"leftBall."]) {
    [self drawSelectionRingForCircleInRect:leftBall];
  } else if ([self.selectedIdentifier hasPrefix:@"rightBall."]) {
    [self drawSelectionRingForCircleInRect:rightBall];
  }

  CGFloat pedalCenterX = 225 * scaleX;
  CGFloat pedalY = 179 * scaleY;
  CGFloat pedalTopHalf = 42 * scaleX;
  CGFloat pedalBottomHalf = 49 * scaleX;
  CGFloat pedalBottomY = 218 * scaleY;
  NSBezierPath *cable = [NSBezierPath bezierPath];
  [cable moveToPoint:NSMakePoint(pedalCenterX, 160 * scaleY)];
  [cable curveToPoint:NSMakePoint(pedalCenterX, pedalY)
        controlPoint1:NSMakePoint(pedalCenterX, 168 * scaleY)
        controlPoint2:NSMakePoint(pedalCenterX, 171 * scaleY)];
  [[NSColor colorWithCalibratedWhite:0.44 alpha:1.0] setStroke];
  cable.lineWidth = 2.0 * scale;
  [cable stroke];
  NSBezierPath *pedal = [NSBezierPath bezierPath];
  [pedal moveToPoint:NSMakePoint(pedalCenterX - pedalTopHalf, pedalY)];
  [pedal lineToPoint:NSMakePoint(pedalCenterX + pedalTopHalf, pedalY)];
  [pedal lineToPoint:NSMakePoint(pedalCenterX + pedalBottomHalf, 211 * scaleY)];
  [pedal curveToPoint:NSMakePoint(pedalCenterX - pedalBottomHalf, 211 * scaleY)
        controlPoint1:NSMakePoint(pedalCenterX, pedalBottomY)
        controlPoint2:NSMakePoint(pedalCenterX, pedalBottomY)];
  [pedal closePath];
  [[NSColor colorWithCalibratedWhite:0.16 alpha:1.0] setFill];
  [pedal fill];
  [[NSColor colorWithCalibratedWhite:0.47 alpha:1.0] setStroke];
  pedal.lineWidth = 1.0 * scale;
  [pedal stroke];
  for (NSInteger tread = 8; tread <= 20; tread += 6) {
    NSBezierPath *line = [NSBezierPath bezierPath];
    CGFloat half = (28 + (tread - 8) * 1.0) * scaleX;
    [line moveToPoint:NSMakePoint(pedalCenterX - half, (pedalY + tread * scaleY))];
    [line lineToPoint:NSMakePoint(pedalCenterX + half, (pedalY + tread * scaleY))];
    [[NSColor colorWithCalibratedWhite:0.47 alpha:0.6] setStroke];
    line.lineWidth = 1.0 * scale;
    [line stroke];
  }
  [self drawCenteredLabel:@"PEDAL"
                  inRect:NSMakeRect(pedalCenterX - 35 * scaleX,
                                    pedalY + 21 * scaleY, 70 * scaleX,
                                    12 * scaleY)
                     font:[NSFont systemFontOfSize:7 * scale
                                             weight:NSFontWeightBold]
                    color:[NSColor colorWithCalibratedWhite:0.82 alpha:1.0]];
  const MuseOnControlMapping *pedalMapping = NULL;
  for (size_t index = 0; index < muse_on_control_mapping_count(); index++) {
    const MuseOnControlMapping *mapping = muse_on_control_mapping_at(index);
    if (mapping->group == MUSE_ON_CONTROL_GROUP_OPTIONAL_PEDAL) {
      pedalMapping = mapping;
      break;
    }
  }
  if (pedalMapping && [self isSelectedMapping:pedalMapping]) {
    [[NSColor controlAccentColor] setStroke];
    pedal.lineWidth = 2.4 * scale;
    [pedal stroke];
  }
}

- (BOOL)acceptsFirstResponder {
  return YES;
}

- (BOOL)canBecomeKeyView {
  return YES;
}

- (void)mouseDown:(NSEvent *)event {
  NSPoint point = [self convertPoint:event.locationInWindow fromView:nil];
  const MuseOnControlMapping *selected = NULL;

  /* Prefer the black keys when a click overlaps their white-key footprint. */
  MuseOnControlGroup priority[] = {
      MUSE_ON_CONTROL_GROUP_BLACK_BUTTONS,
      MUSE_ON_CONTROL_GROUP_WHITE_BUTTONS,
      MUSE_ON_CONTROL_GROUP_TURNTABLE,
      MUSE_ON_CONTROL_GROUP_DIRECTIONAL_BALLS,
      MUSE_ON_CONTROL_GROUP_OPTIONAL_PEDAL};
  for (size_t groupIndex = 0; groupIndex < sizeof(priority) / sizeof(priority[0]);
       groupIndex++) {
    for (size_t index = 0; index < muse_on_control_mapping_count(); index++) {
      const MuseOnControlMapping *mapping = muse_on_control_mapping_at(index);
      if (mapping->group != priority[groupIndex] ||
          !NSPointInRect(point, MuseOnNormalizedRect(mapping, self.bounds.size))) {
        continue;
      }
      selected = mapping;
      break;
    }
    if (selected) break;
  }
  if (selected && self.selectionHandler) {
    self.selectionHandler(MuseOnStringFromUTF8(selected->identifier));
  }
}

- (void)keyDown:(NSEvent *)event {
  unsigned short keyCode = event.keyCode;
  BOOL activatesSelection = keyCode == 36 || keyCode == 49;
  if (activatesSelection && self.selectedIdentifier && self.selectionHandler) {
    self.selectionHandler(self.selectedIdentifier);
    return;
  }
  BOOL movesSelection = keyCode == 123 || keyCode == 124 || keyCode == 125 ||
                        keyCode == 126;
  if (!movesSelection || muse_on_control_mapping_count() == 0) {
    [super keyDown:event];
    return;
  }
  NSInteger currentIndex = -1;
  for (size_t index = 0; index < muse_on_control_mapping_count(); index++) {
    const MuseOnControlMapping *mapping = muse_on_control_mapping_at(index);
    if ([self.selectedIdentifier isEqualToString:
             MuseOnStringFromUTF8(mapping->identifier)]) {
      currentIndex = (NSInteger)index;
      break;
    }
  }
  NSInteger next = currentIndex < 0 ? 0 : currentIndex;
  if (keyCode == 123 || keyCode == 126) next--;
  if (keyCode == 124 || keyCode == 125) next++;
  if (next < 0) next = (NSInteger)muse_on_control_mapping_count() - 1;
  if ((size_t)next >= muse_on_control_mapping_count()) next = 0;
  const MuseOnControlMapping *mapping = muse_on_control_mapping_at((size_t)next);
  if (self.selectionHandler) {
    self.selectionHandler(MuseOnStringFromUTF8(mapping->identifier));
  }
}

@end

static NSString *MuseOnInspectorBadge(const MuseOnControlMapping *mapping) {
  NSString *identifier = MuseOnStringFromUTF8(mapping->identifier);
  if ([identifier hasPrefix:@"white"] || [identifier hasPrefix:@"black"]) {
    return [identifier substringFromIndex:identifier.length - 1];
  }
  if ([identifier hasPrefix:@"turntable."]) return @"TT";
  if ([identifier hasPrefix:@"leftBall."]) return @"LB";
  if ([identifier hasPrefix:@"rightBall."]) return @"RB";
  return @"P";
}

@interface MuseOnControllerMapView : NSView
@property(nonatomic, readonly) NSPopUpButton *selectionPopup;
- (instancetype)initWithProfile:(MuseOnProfile)profile
              selectedIdentifier:(NSString *)selectedIdentifier
                selectionHandler:(void (^)(NSString *identifier))handler;
- (void)selectIdentifier:(NSString *)identifier;
@end

@interface MuseOnControllerMapView ()
@property(nonatomic) MuseOnProfile profile;
@property(nonatomic, strong) MuseOnControllerMapCanvas *canvas;
@property(nonatomic, strong) NSTextField *badgeField;
@property(nonatomic, strong) NSTextField *physicalField;
@property(nonatomic, strong) NSTextField *actionField;
@property(nonatomic, strong) NSTextField *descriptionField;
@property(nonatomic, strong) NSTextField *hintField;
@property(nonatomic, copy) void (^selectionHandler)(NSString *identifier);
@property(nonatomic, copy) NSString *selectedIdentifier;
@end

@implementation MuseOnControllerMapView

- (instancetype)initWithProfile:(MuseOnProfile)profile
              selectedIdentifier:(NSString *)selectedIdentifier
                selectionHandler:(void (^)(NSString *identifier))handler {
  self = [super initWithFrame:NSMakeRect(0, 0, 464, 372)];
  if (!self) return nil;
  _profile = profile;
  _selectionHandler = [handler copy];
  self.selectedIdentifier = selectedIdentifier;
  self.translatesAutoresizingMaskIntoConstraints = NO;
  self.accessibilityRole = NSAccessibilityGroupRole;
  self.accessibilityLabel = @"Muse-On Control Map";
  self.accessibilityValue = @"Visual map with a compact control inspector";

  NSStackView *stack = [NSStackView stackViewWithViews:@[]];
  stack.orientation = NSUserInterfaceLayoutOrientationVertical;
  stack.alignment = NSLayoutAttributeLeading;
  stack.spacing = 7;
  stack.translatesAutoresizingMaskIntoConstraints = NO;
  [self addSubview:stack];

  self.canvas = [[MuseOnControllerMapCanvas alloc] initWithProfile:profile];
  __weak MuseOnControllerMapView *weakSelf = self;
  self.canvas.selectionHandler = ^(NSString *identifier) {
    [weakSelf selectIdentifier:identifier];
    if (weakSelf.selectionHandler) weakSelf.selectionHandler(identifier);
  };
  self.canvas.translatesAutoresizingMaskIntoConstraints = NO;
  [stack addArrangedSubview:self.canvas];
  [self.canvas.widthAnchor constraintEqualToConstant:464].active = YES;
  [self.canvas.heightAnchor constraintEqualToConstant:229].active = YES;

  _selectionPopup = [[NSPopUpButton alloc] initWithFrame:NSZeroRect pullsDown:NO];
  self.selectionPopup.accessibilityLabel = @"Select a Muse-On physical control";
  self.selectionPopup.accessibilityRole = NSAccessibilityPopUpButtonRole;
  for (size_t index = 0; index < muse_on_control_mapping_count(); index++) {
    const MuseOnControlMapping *mapping = muse_on_control_mapping_at(index);
    [self.selectionPopup addItemWithTitle:MuseOnStringFromUTF8(mapping->physical_label)];
  }
  self.selectionPopup.target = self;
  self.selectionPopup.action = @selector(selectionChanged:);
  self.selectionPopup.hidden = YES;

  NSBox *inspector = [[NSBox alloc] initWithFrame:NSZeroRect];
  inspector.title = @"";
  inspector.titleFont = [NSFont systemFontOfSize:10 weight:NSFontWeightSemibold];
  inspector.boxType = NSBoxCustom;
  inspector.transparent = YES;
  inspector.wantsLayer = YES;
  inspector.layer.borderWidth = 1.0;
  inspector.layer.borderColor = [NSColor separatorColor].CGColor;
  inspector.layer.backgroundColor =
      [NSColor colorWithCalibratedWhite:0.18 alpha:1.0].CGColor;
  inspector.layer.cornerRadius = 8.0;
  inspector.contentViewMargins = NSMakeSize(10, 7);
  inspector.translatesAutoresizingMaskIntoConstraints = NO;
  NSStackView *inspectorStack = [NSStackView stackViewWithViews:@[]];
  inspectorStack.orientation = NSUserInterfaceLayoutOrientationVertical;
  inspectorStack.alignment = NSLayoutAttributeLeading;
  inspectorStack.spacing = 1;
  inspectorStack.translatesAutoresizingMaskIntoConstraints = NO;
  [inspector.contentView addSubview:inspectorStack];
  self.badgeField = [NSTextField labelWithString:@""];
  self.badgeField.font = [NSFont systemFontOfSize:11 weight:NSFontWeightBold];
  self.badgeField.alignment = NSTextAlignmentCenter;
  self.badgeField.textColor = [NSColor windowBackgroundColor];
  self.badgeField.wantsLayer = YES;
  self.badgeField.layer.backgroundColor = [NSColor labelColor].CGColor;
  self.badgeField.layer.cornerRadius = 5.0;
  [self.badgeField.widthAnchor constraintEqualToConstant:28].active = YES;
  [self.badgeField.heightAnchor constraintEqualToConstant:22].active = YES;
  self.actionField = [NSTextField labelWithString:@""];
  self.actionField.font = [NSFont systemFontOfSize:13 weight:NSFontWeightSemibold];
  self.physicalField = [NSTextField labelWithString:@""];
  self.physicalField.font = [NSFont systemFontOfSize:10];
  self.physicalField.textColor = [NSColor secondaryLabelColor];
  self.descriptionField = [NSTextField labelWithString:@""];
  self.descriptionField.font = [NSFont systemFontOfSize:11];
  self.descriptionField.textColor = [NSColor secondaryLabelColor];
  self.hintField = [NSTextField labelWithString:
      @"Click or focus any control above; press Enter or Space to inspect it."];
  NSFontDescriptor *hintDescriptor = [[NSFont systemFontOfSize:10].fontDescriptor
      fontDescriptorWithSymbolicTraits:NSFontItalicTrait];
  self.hintField.font = [NSFont fontWithDescriptor:hintDescriptor size:10];
  self.hintField.textColor = [NSColor tertiaryLabelColor];
  NSStackView *inspectorHeader = [NSStackView stackViewWithViews:@[]];
  inspectorHeader.orientation = NSUserInterfaceLayoutOrientationHorizontal;
  inspectorHeader.alignment = NSLayoutAttributeCenterY;
  inspectorHeader.spacing = 8;
  [inspectorHeader addArrangedSubview:self.badgeField];
  [inspectorHeader addArrangedSubview:self.actionField];
  [inspectorStack addArrangedSubview:inspectorHeader];
  for (NSTextField *field in @[self.physicalField, self.descriptionField,
                               self.hintField]) {
    field.accessibilityRole = NSAccessibilityStaticTextRole;
    field.accessibilityLabel = field.stringValue;
    [inspectorStack addArrangedSubview:field];
  }
  [NSLayoutConstraint activateConstraints:@[
      [inspectorStack.leadingAnchor constraintEqualToAnchor:inspector.contentView.leadingAnchor],
      [inspectorStack.trailingAnchor constraintEqualToAnchor:inspector.contentView.trailingAnchor],
      [inspectorStack.topAnchor constraintEqualToAnchor:inspector.contentView.topAnchor],
      [inspectorStack.bottomAnchor constraintEqualToAnchor:inspector.contentView.bottomAnchor],
      [inspector.heightAnchor constraintEqualToConstant:88],
  ]];
  [stack addArrangedSubview:inspector];

  [NSLayoutConstraint activateConstraints:@[
      [stack.leadingAnchor constraintEqualToAnchor:self.leadingAnchor],
      [stack.trailingAnchor constraintEqualToAnchor:self.trailingAnchor],
      [stack.topAnchor constraintEqualToAnchor:self.topAnchor],
      [stack.bottomAnchor constraintEqualToAnchor:self.bottomAnchor],
  ]];

  NSString *initialIdentifier = selectedIdentifier;
  if (!initialIdentifier) {
    initialIdentifier = MuseOnStringFromUTF8(
        muse_on_control_mapping_at(0)->identifier);
  }
  [self selectIdentifier:initialIdentifier];
  return self;
}

- (void)selectionChanged:(NSPopUpButton *)sender {
  NSInteger index = sender.indexOfSelectedItem;
  if (index < 0 || (size_t)index >= muse_on_control_mapping_count()) return;
  const MuseOnControlMapping *mapping = muse_on_control_mapping_at((size_t)index);
  [self selectIdentifier:MuseOnStringFromUTF8(mapping->identifier)];
  if (self.selectionHandler) self.selectionHandler(self.selectedIdentifier);
}

- (void)selectIdentifier:(NSString *)identifier {
  const MuseOnControlMapping *selected = NULL;
  size_t selectedIndex = 0;
  for (size_t index = 0; index < muse_on_control_mapping_count(); index++) {
    const MuseOnControlMapping *mapping = muse_on_control_mapping_at(index);
    if ([identifier isEqualToString:MuseOnStringFromUTF8(mapping->identifier)]) {
      selected = mapping;
      selectedIndex = index;
      break;
    }
  }
  if (!selected) {
    selected = muse_on_control_mapping_at(0);
    selectedIndex = 0;
  }
  self.selectedIdentifier = MuseOnStringFromUTF8(selected->identifier);
  [self.selectionPopup selectItemAtIndex:(NSInteger)selectedIndex];
  self.canvas.selectedIdentifier = self.selectedIdentifier;
  self.canvas.accessibilityValue = [NSString stringWithFormat:
      @"Selected %@. %@. %@ Use arrow keys to move between controls.",
      MuseOnStringFromUTF8(selected->physical_label),
      muse_on_control_mapping_profile(selected, self.profile)->available
          ? MuseOnStringFromUTF8(muse_on_action_display_name(
                muse_on_control_mapping_profile(selected, self.profile)->press_action))
          : @"Not active in selected profile.",
      MuseOnControlDescription(selected, self.profile)];
  self.badgeField.stringValue = MuseOnInspectorBadge(selected);
  self.physicalField.stringValue = MuseOnStringFromUTF8(selected->physical_label);
  self.actionField.stringValue = [NSString stringWithFormat:@"%s",
      muse_on_control_mapping_profile(selected, self.profile)->available
          ? muse_on_action_display_name(
                muse_on_control_mapping_profile(selected, self.profile)->press_action)
          : "Not active in selected profile"];
  self.descriptionField.stringValue = MuseOnControlDescription(selected, self.profile);
  self.badgeField.accessibilityLabel = [NSString stringWithFormat:
      @"Selected control %@", self.badgeField.stringValue];
  self.hintField.accessibilityLabel = self.hintField.stringValue;
  for (NSTextField *field in @[self.physicalField, self.actionField,
                               self.descriptionField, self.hintField]) {
    field.accessibilityLabel = field.stringValue;
  }
  self.canvas.needsDisplay = YES;
}

@end

static NSStackView *MuseOnStatusIndicator(NSString *text, NSColor *color) {
  NSView *dot = [[NSView alloc] initWithFrame:NSZeroRect];
  dot.translatesAutoresizingMaskIntoConstraints = NO;
  dot.wantsLayer = YES;
  dot.layer.backgroundColor = color.CGColor;
  dot.layer.cornerRadius = 3.5;
  [dot.widthAnchor constraintEqualToConstant:7].active = YES;
  [dot.heightAnchor constraintEqualToConstant:7].active = YES;
  NSTextField *label = [NSTextField labelWithString:text];
  label.font = [NSFont systemFontOfSize:11.5 weight:NSFontWeightMedium];
  label.accessibilityRole = NSAccessibilityStaticTextRole;
  label.accessibilityLabel = text;
  NSStackView *indicator = [NSStackView stackViewWithViews:@[dot, label]];
  indicator.orientation = NSUserInterfaceLayoutOrientationHorizontal;
  indicator.alignment = NSLayoutAttributeCenterY;
  indicator.spacing = 5;
  indicator.accessibilityRole = NSAccessibilityStaticTextRole;
  indicator.accessibilityLabel = text;
  return indicator;
}

static NSBox *MuseOnPresentationCard(NSSize margins) {
  NSBox *card = [[NSBox alloc] initWithFrame:NSZeroRect];
  card.boxType = NSBoxCustom;
  card.transparent = YES;
  card.wantsLayer = YES;
  card.layer.backgroundColor =
      [NSColor colorWithCalibratedWhite:0.18 alpha:1.0].CGColor;
  card.layer.borderColor =
      [NSColor colorWithCalibratedWhite:0.29 alpha:1.0].CGColor;
  card.layer.borderWidth = 0.5;
  card.layer.cornerRadius = 9.0;
  card.contentViewMargins = margins;
  card.translatesAutoresizingMaskIntoConstraints = NO;
  return card;
}

@interface MuseOnFlippedDocumentView : NSView
@end

@implementation MuseOnFlippedDocumentView

- (BOOL)isFlipped {
  return YES;
}

@end

static NSScrollView *MuseOnTextEquivalentScrollView(MuseOnProfile profile,
                                                     BOOL expanded) {
  NSScrollView *scroll = [[NSScrollView alloc] initWithFrame:NSZeroRect];
  scroll.translatesAutoresizingMaskIntoConstraints = NO;
  scroll.hasVerticalScroller = YES;
  scroll.hasHorizontalScroller = NO;
  scroll.autohidesScrollers = YES;
  scroll.drawsBackground = NO;
  scroll.borderType = NSNoBorder;
  scroll.accessibilityLabel = @"Complete text equivalent for Control Map";
  scroll.hidden = !expanded;
  [scroll.heightAnchor constraintEqualToConstant:148].active = YES;
  [scroll.widthAnchor constraintEqualToConstant:464].active = YES;

  NSView *document = [[MuseOnFlippedDocumentView alloc]
      initWithFrame:NSMakeRect(0, 0, 464, 420)];
  NSStackView *content = [NSStackView stackViewWithViews:@[]];
  content.orientation = NSUserInterfaceLayoutOrientationVertical;
  content.alignment = NSLayoutAttributeLeading;
  content.spacing = 4;
  content.translatesAutoresizingMaskIntoConstraints = NO;
  [document addSubview:content];

  for (NSInteger groupValue = 0;
       groupValue < MUSE_ON_CONTROL_GROUP_COUNT; groupValue++) {
    MuseOnControlGroup group = (MuseOnControlGroup)groupValue;
    NSTextField *heading = [NSTextField labelWithString:MuseOnStringFromUTF8(
        muse_on_control_group_title(group))];
    heading.font = [NSFont systemFontOfSize:10 weight:NSFontWeightSemibold];
    heading.textColor = [NSColor secondaryLabelColor];
    heading.accessibilityRole = NSAccessibilityStaticTextRole;
    [content addArrangedSubview:heading];

    for (size_t index = 0; index < muse_on_control_mapping_count(); index++) {
      const MuseOnControlMapping *mapping = muse_on_control_mapping_at(index);
      if (mapping->group != group) continue;
      NSTextField *row = [NSTextField labelWithString:MuseOnControlText(mapping,
                                                                         profile)];
      row.font = [NSFont systemFontOfSize:10];
      row.textColor = [NSColor labelColor];
      row.accessibilityRole = NSAccessibilityStaticTextRole;
      row.accessibilityLabel = row.stringValue;
      row.lineBreakMode = NSLineBreakByTruncatingTail;
      row.preferredMaxLayoutWidth = 450;
      [content addArrangedSubview:row];
    }
  }

  [NSLayoutConstraint activateConstraints:@[
      [content.leadingAnchor constraintEqualToAnchor:document.leadingAnchor],
      [content.trailingAnchor constraintEqualToAnchor:document.trailingAnchor],
      [content.topAnchor constraintEqualToAnchor:document.topAnchor],
  ]];
  scroll.documentView = document;
  return scroll;
}

@interface MuseOnAppDelegate : NSObject <NSApplicationDelegate>
@property(nonatomic, strong) NSStatusItem *statusItem;
@property(nonatomic, strong) NSPopover *popover;
@property(nonatomic, strong) MuseOnControllerMapView *controllerMapView;
@property(nonatomic, strong) MuseOnConnectionObserver *connectionObserver;
@property(nonatomic, strong) NSTask *listenerTask;
@property(nonatomic, strong) NSFileHandle *listenerOutputHandle;
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
@property(nonatomic) BOOL listenerPermissionRequired;
@property(nonatomic) MuseOnSafetyFailure listenerSafetyFailure;
@property(nonatomic) ListenerStopPurpose listenerStopPurpose;
@property(nonatomic) BOOL quitInFlight;
@property(nonatomic) BOOL uncleanQuitConfirmationInFlight;
@property(nonatomic) BOOL uncleanQuitAuthorized;
@property(nonatomic) BOOL listenerEverStarted;
@property(nonatomic) BOOL listenerCleanupKnown;
@property(nonatomic) BOOL primaryInstance;
@property(nonatomic) MuseOnDiagnostics diagnostics;
@property(nonatomic) MuseOnSetupState setup;
@property(nonatomic) MuseOnState coordinator;
@property(nonatomic) MuseOnProfile profile;
@property(nonatomic, copy) NSString *selectedControlIdentifier;
@property(nonatomic) BOOL textEquivalentExpanded;
@property(nonatomic) int lockFd;
- (void)applyCoordinatorCommand:(MuseOnCommand)command
               cleanupVerified:(BOOL)cleanupVerified
                 safetyFailure:(MuseOnSafetyFailure)safetyFailure;
- (void)notifySafetyLatchIfAuthorized;
- (void)finishSafeQuit;
- (MuseOnQuitPolicyDecision)quitPolicyDecision;
- (BOOL)confirmUncleanQuitIfNeeded;
- (void)refreshStatusIcon;
- (void)applyConnectionSnapshot:(MuseOnConnectionSnapshot)snapshot;
- (void)startConnectionObserver;
- (void)stopConnectionObserver;
@end

static void MuseOnAppConnectionSnapshot(
    void *context, MuseOnConnectionSnapshot snapshot) {
  MuseOnAppDelegate *delegate = (__bridge MuseOnAppDelegate *)context;
  [delegate applyConnectionSnapshot:snapshot];
}

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
  _permissionGranted = muse_on_input_monitoring_access_granted() &&
                        muse_on_preflight_post_event_access();
  _inputsReleased = NO;
  _filterVerified = true;
  _listenerRecoveryValidated = NO;
  _listenerEverStarted = NO;
  _listenerCleanupKnown = NO;
  _listenerPermissionRequired = NO;
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

- (void)sessionDidChange:(NSNotification *)notification {
  (void)notification;
  self.sessionAvailable = muse_on_session_is_available();
  self.inputsReleased = NO;
  if (!self.sessionAvailable && self.listenerTask &&
      !self.listenerRecoveryMode &&
      self.listenerStopPurpose == kListenerStopNone) {
    self.listenerStopPurpose = kListenerStopForSession;
    [self.listenerTask terminate];
  }
  [self updateCoordinatorWithCommand:MUSE_ON_COMMAND_NONE];
  if (self.sessionAvailable && self.listenerTask == nil &&
      _setup.enabled && !_coordinator.safety_latched && self.permissionGranted) {
    [self startListenerIfNeeded];
  }
  if (self.popover.shown) [self refreshMenu];
}

- (void)saveSetup {
  NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
  [defaults setBool:_setup.enabled forKey:kEnabledIntentKey];
  [defaults setBool:_setup.start_automatically forKey:kStartAutomaticallyKey];
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

- (void)recordListenerError:(NSString *)operation code:(NSNumber *)codeValue {
  const char *raw = operation.UTF8String;
  MuseOnDiagnosticErrorCode error = MUSE_ON_DIAGNOSTIC_ERROR_DEVICE_UNCERTAIN;
  int64_t code = codeValue ? codeValue.longLongValue : 0;

  if (muse_on_listener_error_is_permission_required(raw, (int32_t)code)) {
    self.permissionGranted = NO;
    self.listenerPermissionRequired = YES;
    [self applyCoordinatorCommand:MUSE_ON_COMMAND_NONE
                 cleanupVerified:YES
                   safetyFailure:MUSE_ON_SAFETY_FAILURE_NONE];
    return;
  }

  if (raw && (strcmp(raw, "restore_keyboard_filter") == 0 ||
      strcmp(raw, "rollback_keyboard_filter") == 0 ||
      strcmp(raw, "recover_keyboard_filter") == 0)) {
    error = MUSE_ON_DIAGNOSTIC_ERROR_FILTER_RESTORE;
  } else if (raw && strcmp(raw, "apply_keyboard_filter") == 0) {
    error = MUSE_ON_DIAGNOSTIC_ERROR_FILTER_APPLY;
  } else if (raw && (strcmp(raw, "input_report") == 0 ||
                     strcmp(raw, "invalid_input_report") == 0)) {
    return;
  }
  muse_on_diagnostics_record_listener_error(&_diagnostics, raw, code);
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

  if ([name isEqualToString:@"permission_required"]) {
    self.listenerPermissionRequired = YES;
    self.permissionGranted = NO;
    self.inputsReleased = NO;
    if (!_coordinator.safety_latched) {
      self.listenerSafetyFailure = MUSE_ON_SAFETY_FAILURE_NONE;
    }
    [self applyCoordinatorCommand:MUSE_ON_COMMAND_NONE
                 cleanupVerified:YES
                   safetyFailure:MUSE_ON_SAFETY_FAILURE_NONE];
    if (self.popover.shown) [self refreshMenu];
  } else if ([name isEqualToString:@"tcc_status"] ||
             [name isEqualToString:@"permissions_changed"]) {
    NSString *inputMonitoring = event[@"inputMonitoring"];
    NSNumber *accessibility = event[@"accessibility"];
    if (inputMonitoring) {
      self.permissionGranted =
          [inputMonitoring isEqualToString:@"granted"];
    }
    if (accessibility) self.permissionGranted &= accessibility.boolValue;
    if (!self.permissionGranted) self.inputsReleased = NO;
    [self updateCoordinatorWithCommand:MUSE_ON_COMMAND_NONE];
  } else if ([name isEqualToString:@"ready"]) {
    /* Ready confirms listener setup; it does not prove that an earlier
     * startup error or safety latch was recovered. Retain that tuple until
     * a new attempt begins or recovery is explicitly verified. */
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
    muse_on_diagnostics_record_listener_ready(&_diagnostics);
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
    NSNumber *code = [event[@"code"] isKindOfClass:[NSNumber class]]
                         ? event[@"code"] : nil;
    [self recordListenerError:operation code:code];
  } else if ([name isEqualToString:@"safety_latch"]) {
    self.listenerSawSafetyLatch = YES;
    self.listenerSafetyFailure = [self safetyFailureFromString:safetyReason];
    [self recordSafetyFailure:self.listenerSafetyFailure];
    [self applyCoordinatorCommand:MUSE_ON_COMMAND_NONE
                 cleanupVerified:NO
                   safetyFailure:self.listenerSafetyFailure];
    if (self.popover.shown) [self refreshMenu];
  } else if ([name isEqualToString:@"recovery_state"]) {
    BOOL listenerConnected = [event[@"controllerConnected"] boolValue];
    BOOL listenerMultiple = [event[@"multipleControllers"] boolValue];
    /* The listener may validate ownership, but never becomes the host's
     * connection source. A disagreement keeps Retry fail-closed. */
    self.listenerRecoveryValidated =
        listenerConnected == self.controllerConnected &&
        listenerMultiple == self.multipleControllers;
    self.inputsReleased = [event[@"inputsReleased"] boolValue];
    MuseOnPrerequisites recoveryPrerequisites = {0};
    recoveryPrerequisites.inputs_released = self.inputsReleased;
    muse_on_neutral_entry_require(&recoveryPrerequisites);
    self.inputsReleased = recoveryPrerequisites.inputs_released;
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
    [self recordListenerEvent:event];
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
    if (self.listenerPermissionRequired) {
      cleanupVerified = YES;
    }
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
      if (self.listenerPermissionRequired) {
        self.listenerSafetyFailure = MUSE_ON_SAFETY_FAILURE_NONE;
        [self applyCoordinatorCommand:MUSE_ON_COMMAND_NONE
                     cleanupVerified:YES
                       safetyFailure:MUSE_ON_SAFETY_FAILURE_NONE];
      } else {
        if (cleanupVerified) failure = MUSE_ON_SAFETY_FAILURE_DEVICE_UNCERTAIN;
        [self applyCoordinatorCommand:MUSE_ON_COMMAND_NONE
                     cleanupVerified:cleanupVerified
                       safetyFailure:failure];
      }
      break;
  }
  if (self.popover.shown) [self refreshMenu];
  muse_on_diagnostics_clear_listener_if_recovered(
      &_diagnostics, recoveryMode && self.listenerRecoveryValidated,
      cleanupVerified, self.listenerTask == nil && cleanupVerified,
      _coordinator.safety_latched, self.listenerSafetyFailure);
  if (self.listenerTask == nil) {
    self.listenerStopPurpose = kListenerStopNone;
    self.listenerRecoveryMode = NO;
    self.listenerRecoveryValidated = NO;
    self.listenerPermissionRequired = NO;
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
                          _coordinator.disable_pending ||
                          !self.permissionGranted))) return;
  muse_on_diagnostics_reset_listener(&_diagnostics);
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
  self.listenerPermissionRequired = NO;
  self.listenerOutputBuffer = [NSMutableData data];
  /* Listener startup is not Neutral Entry evidence. Keep the host fail-closed
   * until every selected-profile control has reported released. */
  MuseOnPrerequisites startupPrerequisites = {0};
  startupPrerequisites.inputs_released = self.inputsReleased;
  muse_on_neutral_entry_require(&startupPrerequisites);
  self.inputsReleased = startupPrerequisites.inputs_released;
  self.filterVerified = NO;
  [self updateCoordinatorWithCommand:MUSE_ON_COMMAND_NONE];
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
      NSTaskTerminationReason terminationReason = finishedTask.terminationReason;
      MuseOnDiagnosticTerminationReason diagnosticReason =
          terminationReason == NSTaskTerminationReasonExit
              ? MUSE_ON_DIAGNOSTIC_TERMINATION_EXIT
              : (terminationReason == NSTaskTerminationReasonUncaughtSignal
                     ? MUSE_ON_DIAGNOSTIC_TERMINATION_SIGNAL
                     : MUSE_ON_DIAGNOSTIC_TERMINATION_UNKNOWN);
      BOOL hasTerminationFailure = finishedTask.terminationStatus != 0 ||
                                   diagnosticReason ==
                                       MUSE_ON_DIAGNOSTIC_TERMINATION_SIGNAL ||
                                   weakSelf.listenerSawSafetyLatch ||
                                   weakSelf.diagnostics.listener.has_error;
      if (hasTerminationFailure) {
        MuseOnAppDelegate *strongSelf = weakSelf;
        if (!strongSelf) return;
        muse_on_diagnostics_record_listener_termination(
            &strongSelf->_diagnostics, diagnosticReason,
            finishedTask.terminationStatus,
            diagnosticReason == MUSE_ON_DIAGNOSTIC_TERMINATION_SIGNAL,
            finishedTask.terminationStatus);
      }
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

- (void)applyConnectionSnapshot:(MuseOnConnectionSnapshot)snapshot {
  BOOL nextControllerConnected = snapshot.state == MUSE_ON_CONNECTION_SINGLE;
  BOOL nextMultipleControllers = snapshot.state == MUSE_ON_CONNECTION_MULTIPLE;
  BOOL topologyChanged = self.controllerConnected != nextControllerConnected ||
                         self.multipleControllers != nextMultipleControllers;
  self.controllerConnected = nextControllerConnected;
  self.multipleControllers = nextMultipleControllers;
  if (topologyChanged) self.inputsReleased = NO;
  [self updateCoordinatorWithCommand:MUSE_ON_COMMAND_NONE];
  if (self.popover.shown) [self refreshMenu];
}

- (void)startConnectionObserver {
  if (!self.primaryInstance || self.connectionObserver) return;
  self.connectionObserver = muse_on_connection_observer_create_native(
      MuseOnAppConnectionSnapshot, (__bridge void *)self);
  if (![self.connectionObserver start]) {
    self.controllerConnected = NO;
    self.multipleControllers = NO;
    [self updateCoordinatorWithCommand:MUSE_ON_COMMAND_NONE];
  }
}

- (void)stopConnectionObserver {
  [self.connectionObserver stop];
  self.connectionObserver = nil;
}

- (void)updateCoordinatorWithCommand:(MuseOnCommand)command {
  [self applyCoordinatorCommand:command
               cleanupVerified:YES
                 safetyFailure:self.listenerSafetyFailure];
}

- (NSString *)controlStateTitle {
  switch (_coordinator.status) {
    case MUSE_ON_STATUS_ACTIVE: return @"Active";
    case MUSE_ON_STATUS_INACTIVE: return @"Inactive";
    case MUSE_ON_STATUS_SAFETY_LATCH: return @"Safety";
    case MUSE_ON_STATUS_DISABLED: return @"Disabled";
  }
  return @"Inactive";
}

- (NSString *)controlReasonText {
  switch (_coordinator.status) {
    case MUSE_ON_STATUS_ACTIVE:
      return _profile == MUSE_ON_PROFILE_PEDAL
          ? @"Pedal enabled — foot pedal is Push-to-Talk."
          : @"Controller Only mode.";
    case MUSE_ON_STATUS_DISABLED:
      return @"Disabled by choice.";
    case MUSE_ON_STATUS_SAFETY_LATCH:
      if (_coordinator.disable_pending) {
        return @"Disable pending; cleanup must be verified.";
      }
      if (_coordinator.safety_failure ==
          MUSE_ON_SAFETY_FAILURE_DEVICE_UNCERTAIN) {
        char detail[192];
        size_t written = muse_on_diagnostics_copy_listener_reason(
            &_diagnostics, _coordinator.safety_failure,
            _coordinator.disable_pending, detail, sizeof(detail));
        if (written > 0) {
          return [NSString stringWithFormat:@"Safety latch — %s", detail];
        }
      }
      return [NSString stringWithFormat:@"Safety latch — %s",
                                        muse_on_safety_failure_string(
                                            _coordinator.safety_failure)];
    case MUSE_ON_STATUS_INACTIVE:
      if (_coordinator.inactive_reason == MUSE_ON_INACTIVE_REASON_PERMISSION) {
        return @"Permission required — enable Input Monitoring and Accessibility in Settings.";
      }
      return [NSString stringWithUTF8String:muse_on_inactive_reason_string(
          _coordinator.inactive_reason)];
  }
  return @"Control is unavailable.";
}

- (NSColor *)controlStateColor {
  switch (_coordinator.status) {
    case MUSE_ON_STATUS_ACTIVE: return [NSColor systemGreenColor];
    case MUSE_ON_STATUS_SAFETY_LATCH: return [NSColor systemOrangeColor];
    case MUSE_ON_STATUS_INACTIVE: return [NSColor systemYellowColor];
    case MUSE_ON_STATUS_DISABLED: return [NSColor secondarySystemFillColor];
  }
  return [NSColor secondarySystemFillColor];
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
      stateLabel = [self controlReasonText];
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
  stack.spacing = 7;
  stack.translatesAutoresizingMaskIntoConstraints = NO;

  NSTextField *(^label)(NSString *) = ^NSTextField *(NSString *text) {
    NSTextField *field = [NSTextField labelWithString:text];
    field.lineBreakMode = NSLineBreakByTruncatingTail;
    field.accessibilityLabel = text;
    return field;
  };

  NSStackView *titleRow = [NSStackView stackViewWithViews:@[]];
  titleRow.orientation = NSUserInterfaceLayoutOrientationHorizontal;
  titleRow.alignment = NSLayoutAttributeTop;
  titleRow.spacing = 10;
  NSTextField *logo = label(@"M");
  logo.font = [NSFont systemFontOfSize:20 weight:NSFontWeightBold];
  logo.alignment = NSTextAlignmentCenter;
  logo.textColor = [NSColor whiteColor];
  logo.wantsLayer = YES;
  logo.layer.backgroundColor = [NSColor controlAccentColor].CGColor;
  logo.layer.cornerRadius = 10.0;
  [logo.widthAnchor constraintEqualToConstant:42].active = YES;
  [logo.heightAnchor constraintEqualToConstant:42].active = YES;
  logo.accessibilityLabel = @"Codex Muse-On";
  [titleRow addArrangedSubview:logo];
  NSStackView *titles = [NSStackView stackViewWithViews:@[]];
  titles.orientation = NSUserInterfaceLayoutOrientationVertical;
  titles.alignment = NSLayoutAttributeLeading;
  titles.spacing = 5;
  NSTextField *title = label(@"Codex Muse-On");
  title.font = [NSFont systemFontOfSize:18 weight:NSFontWeightBold];
  [titles addArrangedSubview:title];
  NSBox *statusCard = MuseOnPresentationCard(NSMakeSize(9, 6));
  NSStackView *statusStack = [NSStackView stackViewWithViews:@[]];
  statusStack.orientation = NSUserInterfaceLayoutOrientationVertical;
  statusStack.alignment = NSLayoutAttributeLeading;
  statusStack.spacing = 3;
  NSStackView *statusRow = [NSStackView stackViewWithViews:@[
      MuseOnStatusIndicator(
          self.controllerConnected ? @"Muse-On Connected" : @"Muse-On Disconnected",
          self.controllerConnected ? [NSColor systemGreenColor]
                                    : [NSColor tertiaryLabelColor]),
      MuseOnStatusIndicator([NSString stringWithFormat:@"Control %@",
                                                   [self controlStateTitle]],
                            [self controlStateColor])]];
  statusRow.orientation = NSUserInterfaceLayoutOrientationHorizontal;
  statusRow.alignment = NSLayoutAttributeCenterY;
  statusRow.distribution = NSStackViewDistributionFillEqually;
  statusRow.spacing = 12;
  [statusStack addArrangedSubview:statusRow];
  NSTextField *reason = label([self controlReasonText]);
  reason.font = [NSFont systemFontOfSize:10.5];
  reason.textColor = [NSColor tertiaryLabelColor];
  [statusStack addArrangedSubview:reason];
  [statusCard.contentView addSubview:statusStack];
  statusStack.translatesAutoresizingMaskIntoConstraints = NO;
  [NSLayoutConstraint activateConstraints:@[
      [statusStack.leadingAnchor constraintEqualToAnchor:statusCard.contentView.leadingAnchor],
      [statusStack.trailingAnchor constraintEqualToAnchor:statusCard.contentView.trailingAnchor],
      [statusStack.topAnchor constraintEqualToAnchor:statusCard.contentView.topAnchor],
      [statusStack.bottomAnchor constraintEqualToAnchor:statusCard.contentView.bottomAnchor],
  ]];
  [titles addArrangedSubview:statusCard];
  [titleRow addArrangedSubview:titles];
  [titleRow.widthAnchor constraintEqualToConstant:464].active = YES;
  [titles.widthAnchor constraintEqualToConstant:412].active = YES;
  [statusCard.widthAnchor constraintEqualToConstant:412].active = YES;
  [stack addArrangedSubview:titleRow];

  NSTextField *mapTitle = label(@"Control Map");
  mapTitle.font = [NSFont systemFontOfSize:12 weight:NSFontWeightSemibold];
  [stack addArrangedSubview:mapTitle];

  if (!self.selectedControlIdentifier) {
    self.selectedControlIdentifier = MuseOnDefaultControlIdentifier();
  }
  MuseOnControllerMapView *mapView = [[MuseOnControllerMapView alloc]
      initWithProfile:_profile
      selectedIdentifier:self.selectedControlIdentifier
      selectionHandler:^(NSString *identifier) {
        self.selectedControlIdentifier = identifier;
      }];
  self.controllerMapView = mapView;
  [stack addArrangedSubview:mapView];
  [mapView.widthAnchor constraintEqualToConstant:464].active = YES;
  [mapView.heightAnchor constraintEqualToConstant:324].active = YES;

  NSButton *textEquivalent = [NSButton buttonWithTitle:@""
                                                 target:self
                                                 action:@selector(toggleTextEquivalent:)];
  [textEquivalent setButtonType:NSButtonTypePushOnPushOff];
  textEquivalent.bezelStyle = NSBezelStyleDisclosure;
  textEquivalent.state = self.textEquivalentExpanded ? NSControlStateValueOn
                                                       : NSControlStateValueOff;
  textEquivalent.accessibilityLabel = @"Complete text equivalent for Control Map";
  textEquivalent.accessibilityValue = self.textEquivalentExpanded ? @"Expanded"
                                                                    : @"Collapsed";
  [textEquivalent.widthAnchor constraintEqualToConstant:16].active = YES;
  NSStackView *textEquivalentRow = [NSStackView stackViewWithViews:@[]];
  textEquivalentRow.orientation = NSUserInterfaceLayoutOrientationHorizontal;
  textEquivalentRow.alignment = NSLayoutAttributeCenterY;
  textEquivalentRow.spacing = 4;
  [textEquivalentRow addArrangedSubview:textEquivalent];
  NSTextField *textEquivalentLabel = label(@"Accessible text equivalent");
  textEquivalentLabel.font = [NSFont systemFontOfSize:10 weight:NSFontWeightSemibold];
  [textEquivalentRow addArrangedSubview:textEquivalentLabel];
  [stack addArrangedSubview:textEquivalentRow];
  NSScrollView *textScroll = MuseOnTextEquivalentScrollView(
      _profile, self.textEquivalentExpanded);
  [stack addArrangedSubview:textScroll];

  NSBox *controlCard = MuseOnPresentationCard(NSMakeSize(10, 7));
  NSStackView *controlRow = [NSStackView stackViewWithViews:@[]];
  controlRow.orientation = NSUserInterfaceLayoutOrientationHorizontal;
  controlRow.alignment = NSLayoutAttributeCenterY;
  controlRow.spacing = 8;
  NSTextField *controlLabel = label(@"Muse-On");
  controlLabel.font = [NSFont systemFontOfSize:12 weight:NSFontWeightMedium];
  [controlRow addArrangedSubview:controlLabel];
  NSView *controlSpacer = [[NSView alloc] initWithFrame:NSZeroRect];
  [controlRow addArrangedSubview:controlSpacer];
  NSSegmentedControl *enabledControl =
      [[NSSegmentedControl alloc] initWithFrame:NSZeroRect];
  enabledControl.segmentCount = 2;
  [enabledControl setLabel:@"Disabled" forSegment:0];
  [enabledControl setLabel:@"Enabled" forSegment:1];
  enabledControl.trackingMode = NSSegmentSwitchTrackingSelectOne;
  enabledControl.segmentStyle = NSSegmentStyleRounded;
  enabledControl.selectedSegment = _setup.enabled ? 1 : 0;
  enabledControl.target = self;
  enabledControl.action = @selector(toggleEnabled:);
  enabledControl.accessibilityLabel = @"Codex Muse-On control state";
  enabledControl.accessibilityRole = NSAccessibilityRadioGroupRole;
  enabledControl.enabled = !_coordinator.disable_pending;
  [controlRow addArrangedSubview:enabledControl];
  [enabledControl.widthAnchor constraintEqualToConstant:146].active = YES;
  [controlCard.contentView addSubview:controlRow];
  controlRow.translatesAutoresizingMaskIntoConstraints = NO;
  [NSLayoutConstraint activateConstraints:@[
      [controlRow.leadingAnchor constraintEqualToAnchor:controlCard.contentView.leadingAnchor],
      [controlRow.trailingAnchor constraintEqualToAnchor:controlCard.contentView.trailingAnchor],
      [controlRow.topAnchor constraintEqualToAnchor:controlCard.contentView.topAnchor],
      [controlRow.bottomAnchor constraintEqualToAnchor:controlCard.contentView.bottomAnchor],
  ]];
  [stack addArrangedSubview:controlCard];

  NSBox *profileCard = MuseOnPresentationCard(NSMakeSize(3, 3));
  NSStackView *profileRow = [NSStackView stackViewWithViews:@[]];
  profileRow.orientation = NSUserInterfaceLayoutOrientationHorizontal;
  profileRow.alignment = NSLayoutAttributeCenterY;
  profileRow.spacing = 0;
  NSSegmentedControl *profile = [[NSSegmentedControl alloc] initWithFrame:NSZeroRect];
  profile.segmentCount = 2;
  [profile setLabel:@"Controller Only" forSegment:0];
  [profile setLabel:@"Pedal Enabled" forSegment:1];
  profile.trackingMode = NSSegmentSwitchTrackingSelectOne;
  profile.segmentStyle = NSSegmentStyleRounded;
  profile.selectedSegment = _profile == MUSE_ON_PROFILE_PEDAL ? 1 : 0;
  profile.target = self;
  profile.action = @selector(changeProfile:);
  profile.accessibilityLabel = @"Control Profile";
  profile.accessibilityRole = NSAccessibilityRadioGroupRole;
  [profileRow addArrangedSubview:profile];
  [profile.widthAnchor constraintEqualToConstant:456].active = YES;
  [profileCard.contentView addSubview:profileRow];
  profileRow.translatesAutoresizingMaskIntoConstraints = NO;
  [NSLayoutConstraint activateConstraints:@[
      [profileRow.leadingAnchor constraintEqualToAnchor:profileCard.contentView.leadingAnchor],
      [profileRow.trailingAnchor constraintEqualToAnchor:profileCard.contentView.trailingAnchor],
      [profileRow.topAnchor constraintEqualToAnchor:profileCard.contentView.topAnchor],
      [profileRow.bottomAnchor constraintEqualToAnchor:profileCard.contentView.bottomAnchor],
  ]];
  [stack addArrangedSubview:profileCard];

  NSBox *startupCard = MuseOnPresentationCard(NSMakeSize(10, 7));
  NSStackView *startupRow = [NSStackView stackViewWithViews:@[]];
  startupRow.orientation = NSUserInterfaceLayoutOrientationHorizontal;
  startupRow.alignment = NSLayoutAttributeCenterY;
  NSTextField *startupLabel = label(@"Start Automatically");
  startupLabel.font = [NSFont systemFontOfSize:12];
  [startupRow addArrangedSubview:startupLabel];
  NSView *startupSpacer = [[NSView alloc] initWithFrame:NSZeroRect];
  [startupRow addArrangedSubview:startupSpacer];
  NSButton *startup = [NSButton checkboxWithTitle:@""
                                            target:self action:@selector(toggleStartup:)];
  startup.state = _setup.start_automatically ? NSControlStateValueOn
                                              : NSControlStateValueOff;
  startup.accessibilityLabel = @"Start Automatically";
  startup.accessibilityRole = NSAccessibilityCheckBoxRole;
  [startupRow addArrangedSubview:startup];
  [startupCard.contentView addSubview:startupRow];
  startupRow.translatesAutoresizingMaskIntoConstraints = NO;
  [NSLayoutConstraint activateConstraints:@[
      [startupRow.leadingAnchor constraintEqualToAnchor:startupCard.contentView.leadingAnchor],
      [startupRow.trailingAnchor constraintEqualToAnchor:startupCard.contentView.trailingAnchor],
      [startupRow.topAnchor constraintEqualToAnchor:startupCard.contentView.topAnchor],
  [startupRow.bottomAnchor constraintEqualToAnchor:startupCard.contentView.bottomAnchor],
  ]];
  [startupCard.heightAnchor constraintEqualToConstant:40].active = YES;
  [stack addArrangedSubview:startupCard];

  NSButton *(^actionButton)(NSString *, SEL) = ^NSButton *(NSString *buttonTitle,
                                                            SEL selector) {
    NSButton *button = [NSButton buttonWithTitle:buttonTitle target:self action:selector];
    button.bezelStyle = NSBezelStyleRounded;
    button.font = [NSFont systemFontOfSize:10];
    button.accessibilityLabel = buttonTitle;
    return button;
  };
  NSMutableArray<NSView *> *focusable = [NSMutableArray arrayWithObjects:
      mapView.canvas, textEquivalent, enabledControl,
      profile, startup, nil];
  BOOL permissionNeedsSettings =
      _coordinator.status == MUSE_ON_STATUS_INACTIVE &&
      _coordinator.inactive_reason == MUSE_ON_INACTIVE_REASON_PERMISSION;
  BOOL startupNeedsSettings = [[NSUserDefaults standardUserDefaults]
                                  boolForKey:kStartupApprovalRequiredKey];
  BOOL retryNeeded = permissionNeedsSettings || startupNeedsSettings ||
                     _coordinator.safety_latched || _coordinator.disable_pending;
  NSStackView *conditionalRow = [NSStackView stackViewWithViews:@[]];
  conditionalRow.orientation = NSUserInterfaceLayoutOrientationHorizontal;
  conditionalRow.alignment = NSLayoutAttributeCenterY;
  conditionalRow.spacing = 6;
  if (startupNeedsSettings) {
    NSTextField *approval = label(@"Startup approval required");
    approval.font = [NSFont systemFontOfSize:10];
    approval.textColor = [NSColor secondaryLabelColor];
    [conditionalRow addArrangedSubview:approval];
  }
  if (permissionNeedsSettings) {
    NSButton *settings = actionButton(@"Open Settings…", @selector(openSettings:));
    [conditionalRow addArrangedSubview:settings];
    [focusable addObject:settings];
  }
  if (startupNeedsSettings) {
    NSButton *loginItems = actionButton(@"Open Login Items", @selector(openLoginItems:));
    [conditionalRow addArrangedSubview:loginItems];
    [focusable addObject:loginItems];
  }
  if (retryNeeded) {
    NSButton *retry = actionButton(@"Retry", @selector(retry:));
    [conditionalRow addArrangedSubview:retry];
    [focusable addObject:retry];
  }
  if (conditionalRow.arrangedSubviews.count > 0) [stack addArrangedSubview:conditionalRow];

  NSStackView *supportRow = [NSStackView stackViewWithViews:@[]];
  supportRow.orientation = NSUserInterfaceLayoutOrientationHorizontal;
  supportRow.alignment = NSLayoutAttributeCenterY;
  supportRow.distribution = NSStackViewDistributionFillEqually;
  supportRow.spacing = 5;
  NSButton *copy = actionButton(@"Copy Diagnostics", @selector(copyDiagnostics:));
  NSButton *report = actionButton(@"Report a Problem…", @selector(reportProblem:));
  NSString *quitTitle = (_coordinator.safety_latched ||
                         _coordinator.disable_pending)
      ? @"Quit Anyway…" : @"Quit";
  NSButton *quit = actionButton(quitTitle, @selector(quit:));
  [supportRow addArrangedSubview:copy];
  [supportRow addArrangedSubview:report];
  [supportRow addArrangedSubview:quit];
  [stack addArrangedSubview:supportRow];
  [focusable addObject:copy];
  [focusable addObject:report];
  [focusable addObject:quit];
  for (NSUInteger index = 0; index < focusable.count; index++) {
    focusable[index].nextKeyView = focusable[(index + 1) % focusable.count];
  }

  CGFloat popoverHeight = self.textEquivalentExpanded ? 798 : 650;
  NSView *view = [[NSView alloc] initWithFrame:NSMakeRect(0, 0, 496, popoverHeight)];
  view.appearance = [NSAppearance appearanceNamed:NSAppearanceNameDarkAqua];
  view.accessibilityRole = NSAccessibilityGroupRole;
  view.accessibilityLabel = @"Codex Muse-On status popover";
  [view addSubview:stack];
  [NSLayoutConstraint activateConstraints:@[
      [stack.leadingAnchor constraintEqualToAnchor:view.leadingAnchor constant:16],
      [stack.trailingAnchor constraintEqualToAnchor:view.trailingAnchor constant:-16],
      [stack.topAnchor constraintEqualToAnchor:view.topAnchor constant:12],
      [stack.bottomAnchor constraintLessThanOrEqualToAnchor:view.bottomAnchor constant:-12],
  ]];
  NSViewController *controller = [[NSViewController alloc] init];
  controller.view = view;
  self.popover.contentViewController = controller;
  self.popover.contentSize = NSMakeSize(496, popoverHeight);
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
  if (self.uncleanQuitAuthorized) return;
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
  if ([alert runModal] != NSAlertFirstButtonReturn) {
    [self refreshMenu];
    return;
  }

  NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
  BOOL isFirstEnable = ![defaults boolForKey:kFirstEnableCompletedKey];
  muse_on_setup_state_confirm_first_enable(&_setup);
  [self saveSetup];
  if (isFirstEnable) {
    [defaults setBool:YES forKey:kFirstEnableCompletedKey];
    if (muse_on_should_request_input_monitoring(
            isFirstEnable, muse_on_input_monitoring_access_unknown(),
            NO)) {
      (void)muse_on_request_input_monitoring_access();
    }
    if (!muse_on_preflight_post_event_access()) (void)muse_on_request_post_event_access();
    if (@available(macOS 10.14, *)) {
      [[UNUserNotificationCenter currentNotificationCenter]
          requestAuthorizationWithOptions:UNAuthorizationOptionAlert
                         completionHandler:^(__unused BOOL granted,
                                             __unused NSError *error) {}];
    }
    self.permissionGranted = muse_on_input_monitoring_access_granted() &&
                             muse_on_preflight_post_event_access();
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

- (void)toggleEnabled:(NSSegmentedControl *)sender {
  if (sender.selectedSegment == 1) {
    if (!_setup.enabled) [self enable:sender];
  } else if (_setup.enabled) {
    [self disable:sender];
  }
}

- (void)changeProfile:(NSSegmentedControl *)sender {
  MuseOnProfile next = sender.selectedSegment == 1
      ? MUSE_ON_PROFILE_PEDAL : MUSE_ON_PROFILE_CONTROLLER_ONLY;
  if (_profile == next) return;
  /* SIGTERM invokes the listener's safe release/restoration path first. */
  _profile = next;
  [[NSUserDefaults standardUserDefaults] setObject:
      (_profile == MUSE_ON_PROFILE_PEDAL ? @"pedal" : @"controller-only")
                                         forKey:kControlProfileKey];
  self.inputsReleased = NO;
  [self updateCoordinatorWithCommand:MUSE_ON_COMMAND_NONE];
  if (self.listenerTask) {
    self.listenerStopPurpose = kListenerStopForProfile;
    [self.listenerTask terminate];
  } else {
    [self startListenerIfNeeded];
  }
  [self refreshMenu];
}

- (void)toggleTextEquivalent:(NSButton *)sender {
  self.textEquivalentExpanded = sender.state == NSControlStateValueOn;
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
    self.permissionGranted = muse_on_input_monitoring_access_granted() &&
                             muse_on_preflight_post_event_access();
    MuseOnPrerequisites retryPrerequisites = {0};
    retryPrerequisites.inputs_released = self.inputsReleased;
    muse_on_neutral_entry_require(&retryPrerequisites);
    self.inputsReleased = retryPrerequisites.inputs_released;
    [self refreshMenuWithCommand:MUSE_ON_COMMAND_RETRY];
    if (self.permissionGranted && _setup.enabled &&
        !_coordinator.safety_latched && self.listenerTask == nil) {
      [self startListenerIfNeeded];
    }
    return;
  }
  if (self.listenerTask) {
    [self.listenerTask terminate];
  } else {
    [self startListenerWithSafetyLatch:YES];
  }
  [self refreshMenu];
}

- (MuseOnQuitPolicyDecision)quitPolicyDecision {
  MuseOnQuitPolicyInput input = {
      .safety_latched = _coordinator.safety_latched,
      .disable_pending = _coordinator.disable_pending,
      .unclean_quit_authorized = self.uncleanQuitAuthorized,
      .quit_in_flight = self.quitInFlight,
  };
  return muse_on_quit_policy_decide(input);
}

- (BOOL)confirmUncleanQuitIfNeeded {
  if ([self quitPolicyDecision] != MUSE_ON_QUIT_POLICY_CONFIRM_UNCLEAN_QUIT) {
    return YES;
  }
  if (self.uncleanQuitConfirmationInFlight) return NO;

  self.uncleanQuitConfirmationInFlight = YES;
  NSAlert *alert = [[NSAlert alloc] init];
  alert.messageText = @"Cleanup could not be verified. Held-action release and Pass-through may be uncertain. Control is blocked. Quitting now will require released controls and Retry next launch.";
  [alert addButtonWithTitle:@"Quit Anyway"];
  [alert addButtonWithTitle:@"Stay and Retry"];
  BOOL confirmed = [alert runModal] == NSAlertFirstButtonReturn;
  self.uncleanQuitConfirmationInFlight = NO;
  if (!confirmed) return NO;

  self.uncleanQuitAuthorized = YES;
  return YES;
}

- (void)quit:(id)sender {
  (void)sender;
  if (![self confirmUncleanQuitIfNeeded]) {
    [self refreshMenu];
    return;
  }
  [NSApp terminate:nil];
}

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
  MuseOnQuitPolicyDecision decision;
  (void)sender;
  if (!self.primaryInstance) return NSTerminateNow;
  decision = [self quitPolicyDecision];
  if (decision == MUSE_ON_QUIT_POLICY_WAIT_FOR_IN_FLIGHT_QUIT) {
    return NSTerminateLater;
  }
  if (decision == MUSE_ON_QUIT_POLICY_CONFIRM_UNCLEAN_QUIT) {
    if (![self confirmUncleanQuitIfNeeded]) return NSTerminateCancel;
    decision = [self quitPolicyDecision];
  }
  if (decision == MUSE_ON_QUIT_POLICY_TERMINATE_UNCLEAN_QUIT) {
    self.quitInFlight = YES;
    /* This is the only exceptional cleanup request: the task was launched
     * and is owned by this app. Do not wait for, verify, or claim cleanup. */
    if (self.listenerTask) {
      self.listenerStopPurpose = kListenerStopNone;
      [self.listenerTask terminate];
    }
    return NSTerminateNow;
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
  [self startConnectionObserver];
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
  if (_setup.enabled && !_coordinator.safety_latched && self.permissionGranted) {
    [self startListenerIfNeeded];
  }
}

- (void)applicationWillTerminate:(NSNotification *)notification {
  if (!self.primaryInstance) return;
  [self stopConnectionObserver];
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
