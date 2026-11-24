#pragma once

#include "SettingsFile.h"

#include <QObject>
#include <QString>
#include <QStringList>

namespace omaweb {

// Whether Omaweb has the engine darken a page that has no dark theme of its
// own. A page that declares a dark colour scheme is the engine's to leave
// alone; this is the one decision every engine adapter reads for the rest.
//
// One setting for the whole browser, off by default and independent of the
// Browser theme. It sits in settings.json with the reader's other page
// settings (ADR 0061), so Sync carries it with them. A reader's choice to
// leave one site alone is not kept here but per Space, by the Browser
// controller.
class PageDarkening final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool enabled READ enabled WRITE setEnabled NOTIFY enabledChanged)

public:
    explicit PageDarkening(QString configRoot, QObject *parent = nullptr);

    bool enabled() const;
    void setEnabled(bool enabled);

signals:
    void enabledChanged();

private:
    void load();

    SettingsFile m_settings;
    bool m_enabled = false;
};

// What Omaweb adds to the Qt engine's command line so a darkened page is drawn
// by the role each colour plays: a light ground toward the Browser theme's
// ground, text toward its text, and borders as mid to dark lines, rather than
// with every colour's lightness inverted. Chromium reads the algorithm once per
// renderer, so it is the process's; the colours are each page's.
//
// `engineFlags` is what the environment already passes to the engine, and the
// answer belongs after it. Chromium reads the last `--dark-mode-settings`, so
// the reader's own values are carried into Omaweb's. An algorithm of their own
// is replaced: the cover over a darkened page is drawn with the role mapping,
// and would not match a page drawn with another.
QStringList pageDarkeningEngineFlags(const QStringList &engineFlags);

} // namespace omaweb
