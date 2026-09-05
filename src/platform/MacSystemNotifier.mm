#include "SystemNotifier.h"

#include <QGuiApplication>
#include <QPointer>

#import <Foundation/Foundation.h>
#import <UserNotifications/UserNotifications.h>

namespace omaweb {
namespace {

    // The delegate is the notification centre's, one per process, and it outlives
    // any single notifier. It reports to whichever notifier is current, and to
    // none once that notifier is gone.
    QPointer<SystemNotifier> currentNotifier;

    // UNUserNotificationCenter is a bundle service: it identifies the notifying
    // application by its bundle identifier, and asking for the centre without one
    // raises. A window server is needed too, so an offscreen or minimal platform
    // plugin has nothing to present with.
    bool notificationCentreUsable()
    {
        if (QGuiApplication::platformName() != QLatin1String("cocoa")) {
            return false;
        }
        NSString *identifier = [[NSBundle mainBundle] bundleIdentifier];
        return identifier != nil && identifier.length > 0;
    }

} // namespace
} // namespace omaweb

// Both halves of an answered notification: the reader tapping it, and the
// reader dismissing it. Omaweb's own key rides in the request identifier, so
// nothing here has to keep a table of its own.
@interface OmawebNotificationDelegate : NSObject <UNUserNotificationCenterDelegate>
@end

@implementation OmawebNotificationDelegate

// Without this a notification raised while Omaweb is frontmost is delivered
// silently and never drawn — and the page it belongs to may be in a Space the
// reader is not looking at, which is exactly when it matters.
- (void)userNotificationCenter:(UNUserNotificationCenter *)center
       willPresentNotification:(UNNotification *)notification
         withCompletionHandler:(void (^)(UNNotificationPresentationOptions))completionHandler
{
    (void)center;
    (void)notification;
    completionHandler(
        UNNotificationPresentationOptionBanner | UNNotificationPresentationOptionList);
}

- (void)userNotificationCenter:(UNUserNotificationCenter *)center
    didReceiveNotificationResponse:(UNNotificationResponse *)response
             withCompletionHandler:(void (^)(void))completionHandler
{
    (void)center;
    const QString key = QString::fromNSString(response.notification.request.identifier);
    const NSString *action = response.actionIdentifier;
    const bool dismissed = [action isEqualToString:UNNotificationDismissActionIdentifier];
    if (auto *notifier = omaweb::currentNotifier.data()) {
        // The answer arrives on the main queue, which is the Qt main thread, so
        // the signal is emitted straight rather than posted.
        if (dismissed) {
            emit notifier->dismissed(key);
        } else {
            emit notifier->activated(key);
        }
    }
    completionHandler();
}

@end

namespace omaweb {

SystemNotifier::SystemNotifier(QObject *parent)
    : QObject(parent)
{
    if (!notificationCentreUsable()) {
        return;
    }
    currentNotifier = this;
    static OmawebNotificationDelegate *delegate = nil;
    @autoreleasepool {
        UNUserNotificationCenter *centre = [UNUserNotificationCenter currentNotificationCenter];
        if (!delegate) {
            // This file is compiled without ARC, as the rest of Omaweb's AppKit
            // code is. The delegate is the centre's for the life of the
            // process and is deliberately never released.
            delegate = [[OmawebNotificationDelegate alloc] init];
        }
        centre.delegate = delegate;
        // Asking costs the reader one system prompt, once. A refusal is not an
        // error: `present` then puts nothing on screen, and the page goes
        // unanswered, which is what a refusal means.
        [centre requestAuthorizationWithOptions:UNAuthorizationOptionAlert
                              completionHandler:^(BOOL granted, NSError *error) {
                                  (void)granted;
                                  (void)error;
                              }];
    }
}

SystemNotifier::~SystemNotifier()
{
    if (currentNotifier == this) {
        currentNotifier.clear();
    }
}

bool SystemNotifier::available() const { return notificationCentreUsable(); }

bool SystemNotifier::present(const QString &key, const QString &title, const QString &body)
{
    if (!available() || key.isEmpty()) {
        return false;
    }
    @autoreleasepool {
        UNMutableNotificationContent *content = [[UNMutableNotificationContent alloc] init];
        content.title = title.toNSString();
        content.body = body.toNSString();
        UNNotificationRequest *request =
            [UNNotificationRequest requestWithIdentifier:key.toNSString()
                                                 content:content
                                                 trigger:nil];
        [[UNUserNotificationCenter currentNotificationCenter] addNotificationRequest:request
                                                               withCompletionHandler:nil];
        [content release];
    }
    return true;
}

void SystemNotifier::withdraw(const QString &key)
{
    if (!available() || key.isEmpty()) {
        return;
    }
    @autoreleasepool {
        NSArray<NSString *> *identifiers = @[ key.toNSString() ];
        UNUserNotificationCenter *centre = [UNUserNotificationCenter currentNotificationCenter];
        [centre removePendingNotificationRequestsWithIdentifiers:identifiers];
        [centre removeDeliveredNotificationsWithIdentifiers:identifiers];
    }
}

} // namespace omaweb
