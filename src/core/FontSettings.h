#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantMap>

#include <optional>

namespace omaweb {

// The reader's decisions about type, both the size Omaweb's own interface is
// drawn at and the fonts a page gets when it names none.
//
// Each is an override and not a new default. The theme sets the interface
// size (ADR 0018) and the engine sets a page's fonts, and this holds what the
// reader said on top of them: an interface size that stands until reset, and
// a page family or size that reads as the engine's until one is named. So a
// theme switch changes the interface size only while no override stands, and
// resetting a value returns it to whatever the theme or the engine would do
// now rather than to a number remembered from when it was set.
//
// One setting for the whole browser, kept with the reader's other decisions
// in the configuration directory (ADR 0016). A Space's session store cannot
// hold it: a Private window has no store, and its type and its pages are the
// same reader's to read.
class FontSettings final : public QObject {
    Q_OBJECT
    Q_PROPERTY(int interfaceFontSize READ interfaceFontSize NOTIFY interfaceFontSizeChanged)
    Q_PROPERTY(bool interfaceFontSizeOverridden READ interfaceFontSizeOverridden NOTIFY
            interfaceFontSizeChanged)
    Q_PROPERTY(
        int themeFontSize READ themeFontSize WRITE setThemeFontSize NOTIFY interfaceFontSizeChanged)
    Q_PROPERTY(int minimumInterfaceFontSize READ minimumInterfaceSize CONSTANT)
    Q_PROPERTY(int maximumInterfaceFontSize READ maximumInterfaceSize CONSTANT)
    Q_PROPERTY(QVariantMap pageFontsMap READ pageFontsMap NOTIFY pageFontsChanged)

public:
    enum class PageFamily { Standard, Fixed };
    Q_ENUM(PageFamily)
    enum class PageSize { Default, Minimum };
    Q_ENUM(PageSize)

    // What a page is drawn with: the reader's value where one stands and the
    // engine's own where none does.
    struct PageFonts {
        QString standardFamily;
        QString fixedFamily;
        int fontSize = 0;
        int minimumFontSize = 0;
    };

    // Twice the kit's default base size at the top, and at the bottom the
    // smallest the kit's caption token still draws a legible glyph at.
    static constexpr int minimumInterfaceFontSize = 8;
    static constexpr int maximumInterfaceFontSize = 24;
    // A page's default size is a size a page is read at, and its minimum is a
    // floor under one; a floor above the size it floors would read as a page
    // that ignores its own size. Chromium offers the same two ranges.
    static constexpr int minimumPageFontSize = 9;
    static constexpr int maximumPageFontSize = 72;
    static constexpr int maximumPageMinimumFontSize = 24;

    // The families offered are the ones the host has, handed in so that a
    // test can name a host of its own and the core needs no font database.
    explicit FontSettings(
        QString configRoot, QStringList installedFamilies, QObject *parent = nullptr);

    // The size the interface is drawn at: the reader's, or the theme's while
    // the reader has not said.
    int interfaceFontSize() const;
    bool interfaceFontSizeOverridden() const;
    int themeFontSize() const;
    // The active theme's size, which is what the interface takes without an
    // override and what reset returns to.
    void setThemeFontSize(int size);
    // Every step is one pixel of base size; the kit derives the scale from it.
    Q_INVOKABLE void setInterfaceFontSize(int size);
    Q_INVOKABLE void increaseInterfaceFontSize();
    Q_INVOKABLE void decreaseInterfaceFontSize();
    Q_INVOKABLE void resetInterfaceFontSize();

    PageFonts pageFonts() const;
    // The same answer for QML, with each value beside whether the reader set
    // it and what the engine would do instead.
    QVariantMap pageFontsMap() const;
    bool pageFamilyOverridden(PageFamily which) const;
    bool pageSizeOverridden(PageSize which) const;
    // An empty family or a zero size is the engine's own again. A family the
    // host does not have is kept, in case it comes back, but reads as unset.
    Q_INVOKABLE void setPageFamily(PageFamily which, const QString &family);
    Q_INVOKABLE void setPageSize(PageSize which, int size);
    // What the engine draws a page with when nothing is set, reported by the
    // engine adapter once it has an engine to ask.
    void setEngineFonts(const PageFonts &fonts);
    Q_INVOKABLE QStringList installedFamilies() const;

signals:
    void interfaceFontSizeChanged();
    void pageFontsChanged();

private:
    static int minimumInterfaceSize() { return minimumInterfaceFontSize; }
    static int maximumInterfaceSize() { return maximumInterfaceFontSize; }
    QString installedFamily(const QString &family) const;
    void load();
    void save() const;

    QString m_configRoot;
    QStringList m_installedFamilies;
    int m_themeFontSize = 12;
    std::optional<int> m_interfaceFontSize;
    PageFonts m_engineFonts;
    // The reader's page fonts as stored: empty and zero where nothing is set.
    PageFonts m_pageFonts;
};

} // namespace omaweb
