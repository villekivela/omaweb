#pragma once

#include <QObject>
#include <QPointer>

#include <vector>

class QWebEngineSettings;

namespace omaweb {

class FontSettings;

// The reader's page fonts, set on every Engine profile QtWebEngine runs: a
// Space's and the Private windows' shared one alike. A profile's settings are
// the root every view of it inherits from, so a change reaches every open
// page of every attached profile at once. A family or a default size has the
// engine restyle the pages already showing; the minimum size alone does not,
// and lands on a page's next layout.
//
// Qt's Quick profile keeps its font settings behind a private class, and this
// is the one place Omaweb reaches past that (ADR 0047). A Qt other than the
// one this was compiled against is not reached into: `available()` reads
// false, the engine draws with its own defaults, and the Settings group says
// so.
class QtPageFonts final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool available READ available CONSTANT)

public:
    explicit QtPageFonts(FontSettings *settings, QObject *parent = nullptr);

    bool available() const;

public slots:
    // Called for every profile Content blocking attaches to, which is every
    // profile there is. The first profile also answers what the engine's own
    // fonts are, before anything is written over them.
    void attachToProfile(QObject *profile);

private:
    void apply();
    void applyTo(QWebEngineSettings *settings) const;
    void readEngineFonts(const QWebEngineSettings *settings);

    FontSettings *m_settings;
    bool m_engineFontsRead = false;
    // The engine's own default and fixed sizes, kept apart so that a reader's
    // default size moves the fixed size by the engine's own distance.
    int m_engineFontSize = 0;
    int m_engineFixedFontSize = 0;
    std::vector<QPointer<QObject>> m_profiles;
};

} // namespace omaweb
