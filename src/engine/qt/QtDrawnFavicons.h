#pragma once

#include "SpaceStorage.h"

namespace omaweb {

// Removes the favicons the engine drew in earlier runs from every Space's
// Engine profile, so a run draws a site's icon under its own colour scheme
// (ADR 0065). Call before any profile is built: the engine keeps the database
// open from then on.
void forgetDrawnFavicons(const SpaceStorage &storage);

} // namespace omaweb
