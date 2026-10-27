#include "TouchpadHold.h"

namespace omaweb {

// macOS sends the glide itself, as `ScrollMomentum`, and ends it when the fingers come down.
struct TouchpadHold::Native { };

TouchpadHold::TouchpadHold(QObject *parent)
    : QObject(parent)
{
}

TouchpadHold::~TouchpadHold() = default;

bool TouchpadHold::listening() const { return false; }

} // namespace omaweb
