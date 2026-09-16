#include "FontSettings.h"

#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QQmlEngine>
#include <QSaveFile>

#include <algorithm>
#include <utility>

namespace omaweb {
namespace {

    // The file is named for the Settings section it belongs to rather than
    // for any one key, the way the privacy section's is.
    constexpr auto fileName = "interface.json";
    constexpr auto interfaceSizeKey = "font-size";
    constexpr auto pageFontsKey = "page-fonts";
    constexpr auto standardFamilyKey = "standard-family";
    constexpr auto fixedFamilyKey = "fixed-family";
    constexpr auto pageSizeKey = "font-size";
    constexpr auto pageMinimumSizeKey = "minimum-font-size";

    int sizeOrZero(const QJsonValue &value)
    {
        return value.isDouble() ? std::max(0, value.toInt()) : 0;
    }

} // namespace

FontSettings::FontSettings(QString configRoot, QStringList installedFamilies, QObject *parent)
    : QObject(parent)
    , m_configRoot(std::move(configRoot))
    , m_installedFamilies(std::move(installedFamilies))
{
    load();
}

int FontSettings::interfaceFontSize() const
{
    return m_interfaceFontSize.value_or(m_themeFontSize);
}

bool FontSettings::interfaceFontSizeOverridden() const { return m_interfaceFontSize.has_value(); }

int FontSettings::themeFontSize() const { return m_themeFontSize; }

void FontSettings::setThemeFontSize(int size)
{
    if (size == m_themeFontSize) {
        return;
    }
    m_themeFontSize = size;
    emit themeFontSizeChanged();
    if (!m_interfaceFontSize) {
        emit interfaceFontSizeChanged();
    }
}

void FontSettings::setInterfaceFontSize(int size)
{
    const auto clamped = std::clamp(size, minimumInterfaceFontSize, maximumInterfaceFontSize);
    if (m_interfaceFontSize == clamped) {
        return;
    }
    m_interfaceFontSize = clamped;
    save();
    emit interfaceFontSizeChanged();
}

void FontSettings::increaseInterfaceFontSize() { setInterfaceFontSize(interfaceFontSize() + 1); }

void FontSettings::decreaseInterfaceFontSize() { setInterfaceFontSize(interfaceFontSize() - 1); }

void FontSettings::resetInterfaceFontSize()
{
    if (!m_interfaceFontSize) {
        return;
    }
    m_interfaceFontSize.reset();
    save();
    emit interfaceFontSizeChanged();
}

FontSettings::PageFonts FontSettings::pageFonts() const
{
    const auto standard = installedFamily(m_pageFonts.standardFamily);
    const auto fixed = installedFamily(m_pageFonts.fixedFamily);
    return {
        standard.isEmpty() ? m_engineFonts.standardFamily : standard,
        fixed.isEmpty() ? m_engineFonts.fixedFamily : fixed,
        m_pageFonts.fontSize > 0 ? m_pageFonts.fontSize : m_engineFonts.fontSize,
        m_pageFonts.minimumFontSize > 0 ? m_pageFonts.minimumFontSize
                                        : m_engineFonts.minimumFontSize,
    };
}

QVariantMap FontSettings::pageFontsMap() const
{
    const auto fonts = pageFonts();
    const auto entry = [](const QVariant &value, bool overridden, const QVariant &engine) {
        return QVariantMap {
            {QStringLiteral("value"), value},
            {QStringLiteral("overridden"), overridden},
            {QStringLiteral("engine"), engine},
        };
    };
    return {
        {QStringLiteral("standardFamily"),
            entry(fonts.standardFamily, pageFamilyOverridden(PageFamily::Standard),
                m_engineFonts.standardFamily)},
        {QStringLiteral("fixedFamily"),
            entry(fonts.fixedFamily, pageFamilyOverridden(PageFamily::Fixed),
                m_engineFonts.fixedFamily)},
        {QStringLiteral("fontSize"),
            entry(fonts.fontSize, pageSizeOverridden(PageSize::Default), m_engineFonts.fontSize)},
        {QStringLiteral("minimumFontSize"),
            entry(fonts.minimumFontSize, pageSizeOverridden(PageSize::Minimum),
                m_engineFonts.minimumFontSize)},
    };
}

bool FontSettings::pageFamilyOverridden(PageFamily which) const
{
    return !installedFamily(
        which == PageFamily::Standard ? m_pageFonts.standardFamily : m_pageFonts.fixedFamily)
                .isEmpty();
}

bool FontSettings::pageSizeOverridden(PageSize which) const
{
    return (which == PageSize::Default ? m_pageFonts.fontSize : m_pageFonts.minimumFontSize) > 0;
}

void FontSettings::setPageFamily(PageFamily which, const QString &family)
{
    auto &stored
        = which == PageFamily::Standard ? m_pageFonts.standardFamily : m_pageFonts.fixedFamily;
    if (stored == family) {
        return;
    }
    stored = family;
    save();
    emit pageFontsChanged();
}

void FontSettings::setPageSize(PageSize which, int size)
{
    auto &stored = storedSize(which);
    const auto clamped = size <= 0   ? 0
        : which == PageSize::Default ? std::clamp(size, minimumPageFontSize, maximumPageFontSize)
                                     : std::min(size, maximumPageMinimumFontSize);
    if (stored == clamped) {
        return;
    }
    stored = clamped;
    save();
    emit pageFontsChanged();
}

void FontSettings::setEngineFonts(const PageFonts &fonts)
{
    m_engineFonts = fonts;
    emit pageFontsChanged();
}

QStringList FontSettings::installedFamilies() const { return m_installedFamilies; }

QString &FontSettings::storedFamily(PageFamily which)
{
    return which == PageFamily::Standard ? m_pageFonts.standardFamily : m_pageFonts.fixedFamily;
}

const QString &FontSettings::storedFamily(PageFamily which) const
{
    return which == PageFamily::Standard ? m_pageFonts.standardFamily : m_pageFonts.fixedFamily;
}

int &FontSettings::storedSize(PageSize which)
{
    return which == PageSize::Default ? m_pageFonts.fontSize : m_pageFonts.minimumFontSize;
}

int FontSettings::storedSize(PageSize which) const
{
    return which == PageSize::Default ? m_pageFonts.fontSize : m_pageFonts.minimumFontSize;
}

QString FontSettings::installedFamily(const QString &family) const
{
    return m_installedFamilies.contains(family) ? family : QString {};
}

// A file that cannot be read the way it is written sets nothing: only a value
// in the shape this writes is a value the reader chose.
void FontSettings::load()
{
    m_interfaceFontSize.reset();
    m_pageFonts = {};
    if (m_configRoot.isEmpty()) {
        return;
    }
    QFile file(QDir(m_configRoot).filePath(QLatin1String(fileName)));
    if (!file.open(QIODevice::ReadOnly)) {
        return;
    }
    const auto object = QJsonDocument::fromJson(file.readAll()).object();
    const auto interfaceSize = object.value(QLatin1String(interfaceSizeKey));
    if (interfaceSize.isDouble()) {
        m_interfaceFontSize
            = std::clamp(interfaceSize.toInt(), minimumInterfaceFontSize, maximumInterfaceFontSize);
    }
    const auto page = object.value(QLatin1String(pageFontsKey)).toObject();
    m_pageFonts.standardFamily = page.value(QLatin1String(standardFamilyKey)).toString();
    m_pageFonts.fixedFamily = page.value(QLatin1String(fixedFamilyKey)).toString();
    m_pageFonts.fontSize = sizeOrZero(page.value(QLatin1String(pageSizeKey)));
    m_pageFonts.minimumFontSize = sizeOrZero(page.value(QLatin1String(pageMinimumSizeKey)));
}

void FontSettings::save() const
{
    if (m_configRoot.isEmpty() || !QDir().mkpath(m_configRoot)) {
        return;
    }
    QSaveFile file(QDir(m_configRoot).filePath(QLatin1String(fileName)));
    if (!file.open(QIODevice::WriteOnly)) {
        return;
    }
    // Only what the reader set is written, so the file reads as the list of
    // their decisions rather than a copy of the defaults.
    QJsonObject page;
    if (!m_pageFonts.standardFamily.isEmpty()) {
        page.insert(QLatin1String(standardFamilyKey), m_pageFonts.standardFamily);
    }
    if (!m_pageFonts.fixedFamily.isEmpty()) {
        page.insert(QLatin1String(fixedFamilyKey), m_pageFonts.fixedFamily);
    }
    if (m_pageFonts.fontSize > 0) {
        page.insert(QLatin1String(pageSizeKey), m_pageFonts.fontSize);
    }
    if (m_pageFonts.minimumFontSize > 0) {
        page.insert(QLatin1String(pageMinimumSizeKey), m_pageFonts.minimumFontSize);
    }
    QJsonObject object;
    if (m_interfaceFontSize) {
        object.insert(QLatin1String(interfaceSizeKey), *m_interfaceFontSize);
    }
    if (!page.isEmpty()) {
        object.insert(QLatin1String(pageFontsKey), page);
    }
    file.write(QJsonDocument(object).toJson(QJsonDocument::Indented));
    file.commit();
}

void registerFontSettings()
{
    qmlRegisterUncreatableType<FontSettings>("Omaweb", 1, 0, "FontSettings",
        QStringLiteral("The reader's font settings are handed to QML, not built there."));
}

} // namespace omaweb
