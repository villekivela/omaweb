#pragma once

#include <QStringList>

namespace omaweb {

// The Chromium the feature name below was checked against, which is the major
// version of the approved engine baseline in `security/baseline.json`. The name
// is Chromium's and has changed between versions, so a moved baseline is when
// to look at it again.
constexpr int checkedChromiumMajorVersion = 140;

// What Omaweb adds to the engine command line so video decodes on the GPU
// rather than in software. VA-API is a Linux interface, so this is the caller's
// to ask for only where the host could answer it.
//
// `engineFlags` is what the environment already passes to the engine, and the
// answer belongs after it on the command line. The answer is empty where the
// host said no: with the GPU process or accelerated decoding turned off there
// is nowhere to decode, and naming the feature among the disabled ones is the
// way to refuse it on its own.
//
// Nothing here weakens the sandbox or a web security boundary, and the caller
// audits the answer the way it audits the environment's flags, so both routes
// in are held to one rule.
//
// A host with no working VA-API driver needs no configuration: Chromium finds
// no driver, decodes in software, and starts as it did before.
QStringList hardwareVideoDecodeFlags(const QStringList &engineFlags);

} // namespace omaweb
