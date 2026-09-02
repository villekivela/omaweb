#pragma once

#include <QObject>

namespace omaweb {

// What one process is costing, asked of the operating system. A retained tab
// keeps a renderer running while the reader is looking at another Space, and
// Omaweb has to be able to say what that costs rather than only that it is
// happening. The engine adapter reports which process draws a page; this
// answers for that process.
//
// Resident memory is what is asked for because it is what the reader is paying:
// pages held in physical memory for a Space they are not looking at.
class ProcessResources final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool available READ available CONSTANT)

public:
    explicit ProcessResources(QObject *parent = nullptr);

    bool available() const;

    // Bytes resident, or zero where the platform cannot say and for a process
    // that is not there — a page with no renderer costs nothing, and reporting
    // a made-up number would be worse than reporting none.
    Q_INVOKABLE qint64 residentBytes(qint64 pid) const;
};

// Makes `ProcessResources` available to QML as `import Omaweb`. Call once per
// process, before loading QML that uses it.
void registerProcessResources();

} // namespace omaweb
