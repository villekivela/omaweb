#pragma once

#include <QStringList>

class QObject;

namespace omaweb {

// What Content blocking owes QML, checked the way the engine view's own
// contract is.
//
// Nothing here is checked by the compiler. A rename used to be caught only by
// whichever caller happened to be exercised, and the hand-written fakes that
// stand in for a blocker went on answering nothing at all (#142). Validating
// against these lists is what turns that into a failing test.
//
// The surface has two halves, because it has two kinds of caller.

// What an engine adapter asks. Every fake is held to this: a benchmark's
// counting stub, an inline QML object, the UI lab's.
QStringList validateEngineBlockerContract(const QObject &blocker);

// What the chrome asks on top, to state the Refusal tally for the address it is
// showing. Only ever asked of the real blocker, and checked so that a rename on
// this side fails a test rather than leaving three panels reading zero.
QStringList validateChromeBlockerContract(const QObject &blocker);

} // namespace omaweb
