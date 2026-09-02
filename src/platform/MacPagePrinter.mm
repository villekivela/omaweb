#include "PagePrinter.h"

#include <QGuiApplication>

#import <AppKit/AppKit.h>
#import <Quartz/Quartz.h>

namespace omaweb {

// The print panel is a window-server service. Offscreen and minimal platform
// plugins have no window server, so there is no panel to present and the
// command says so rather than blocking on a dialog nobody can answer.
bool PagePrinter::available() const
{
    return QGuiApplication::platformName() == QLatin1String("cocoa");
}

// AppKit's print panel is the operating system's own, and its PDF menu is the
// PDF destination the reader expects. The page has already been rendered to
// PDF by the engine adapter, so what is printed is exactly what was on screen.
bool PagePrinter::present(const QString &path, const QString &jobName)
{
    if (!available() || path.isEmpty()) {
        discard(path);
        return false;
    }

    bool presented = false;
    @autoreleasepool {
        // This file is compiled without ARC, as the rest of Omaweb's AppKit code
        // is, so what is allocated or copied here is released here.
        NSURL *location = [NSURL fileURLWithPath:path.toNSString()];
        PDFDocument *document = [[PDFDocument alloc] initWithURL:location];
        if (document) {
            NSPrintInfo *info = [[NSPrintInfo sharedPrintInfo] copy];
            info.jobDisposition = NSPrintSpoolJob;
            NSPrintOperation *operation =
                [document printOperationForPrintInfo:info
                                         scalingMode:kPDFPrintPageScaleDownToFit
                                          autoRotate:YES];
            if (operation) {
                operation.jobTitle = jobName.isEmpty()
                    ? @"Omaweb" : jobName.toNSString();
                operation.showsPrintPanel = YES;
                operation.showsProgressPanel = YES;
                // Modal by design: the reader is answering a question about
                // this page, and the answer decides whether it is printed.
                [operation runOperation];
                presented = true;
            }
            [info release];
            [document release];
        }
    }
    // The rendered copy has served its purpose either way; a document the
    // reader wanted to keep is a download, not a leftover in a spool.
    discard(path);
    return presented;
}

} // namespace omaweb
