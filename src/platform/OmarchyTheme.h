#pragma once

#include <QByteArray>
#include <QString>

namespace omaweb {

// The two directories Omarchy owns, passed in rather than resolved inside so a
// test can point the whole exchange at a temporary home.
struct OmarchyThemePaths {
    // `~/.config/omarchy`: the reader's own configuration, including the
    // `themed/` templates every theme is rendered through.
    QString configuration;
    // `~/.local/state/omarchy`: what Omarchy generates, and the evidence that
    // Omarchy is installed at all.
    QString state;

    static OmarchyThemePaths fromEnvironment();

    // The template Omaweb wants to be there, and the palette Omarchy renders
    // it into for whichever theme is active.
    QString userTemplate() const;
    QString renderedTheme() const;
};

// What following the desktop's theme came to. A decision a reader would want
// to know about is logged where it happens -- the silent ones are the two that
// wrote nothing. The value is what a test reads.
enum class OmarchyTemplateOutcome {
    // There is no Omarchy on this machine. Nothing was written and nothing is
    // wrong -- this is macOS, and every desktop that is not Omarchy.
    Absent,
    // The reader manages `~/.config/omarchy` themselves and said so.
    Declined,
    // The template was not there and now is.
    Installed,
    // A template was already there, so whatever it says stands.
    Kept,
    // Omarchy is installed and the template could not be written.
    Failed,
};

// A file's contents, or none for a path that is empty or cannot be read.
QByteArray readFile(const QString &path);

// Writes a file Omaweb puts in the reader's home, such as a template it ships
// into their Omarchy configuration, readable by everyone and theirs to edit. A
// shipped template can come out of the binary's own read-only resources, so it
// is written rather than copied: a copy would carry the resource's permissions
// onto disk and hand the reader a file they cannot edit.
bool writeReaderFile(const QString &path, const QByteArray &contents);

// Asks Omarchy to render the theme that is already active, which is what turns
// a template just installed into a file. The render starts when the event loop
// next runs, and every request made before then is that one render. Does
// nothing without Omarchy.
void renderActiveOmarchyTheme();

// Put Omaweb's theme template where Omarchy renders templates from, and ask
// Omarchy to render the active theme through it if it has not already. Costs
// one file write on the first run of a new install and nothing after that.
OmarchyTemplateOutcome followOmarchyTheme(
    const OmarchyThemePaths &paths, const QString &shippedTemplate);

} // namespace omaweb
