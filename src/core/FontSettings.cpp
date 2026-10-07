#include "FontSettings.h"

#include <QJsonObject>
#include <QQmlEngine>

#include <algorithm>
#include <utility>

namespace omaweb {
namespace {

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
    , m_settings(std::move(configRoot))
    , m_installedFamilies(std::move(installedFamilies))
{
    connect(&m_settings, &SettingsFile::changed, this, [this](const QStringList &keys) {
        if (!keys.contains(QLatin1String(interfaceSizeKey))
            && !keys.contains(QLatin1String(pageFontsKey))) {
            return;
        }
        const auto interfaceSize = interfaceFontSize();
        const auto stored = m_pageFonts;
        load();
        if (interfaceFontSize() != interfaceSize) {
            emit interfaceFontSizeChanged();
        }
        if (m_pageFonts != stored) {
            emit pageFontsChanged();
        }
    });
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

// Each setter writes the one value it changes, and the file's change handler
// applies it, so a write the file refused changes nothing and a member the
// file gained before this instance read it is kept. A refused write says so,
// which draws a control the reader moved back where the value stands.
void FontSettings::setInterfaceFontSize(int size)
{
    const auto clamped = std::clamp(size, minimumInterfaceFontSize, maximumInterfaceFontSize);
    if (m_interfaceFontSize == clamped) {
        return;
    }
    if (!m_settings.set(QLatin1String(interfaceSizeKey), clamped)) {
        emit interfaceFontSizeChanged();
    }
}

void FontSettings::increaseInterfaceFontSize() { setInterfaceFontSize(interfaceFontSize() + 1); }

void FontSettings::decreaseInterfaceFontSize() { setInterfaceFontSize(interfaceFontSize() - 1); }

void FontSettings::resetInterfaceFontSize()
{
    if (!m_interfaceFontSize) {
        return;
    }
    if (!m_settings.set(QLatin1String(interfaceSizeKey), QJsonValue())) {
        emit interfaceFontSizeChanged();
    }
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
    if (storedFamily(which) == family) {
        return;
    }
    if (!m_settings.setMember(QLatin1String(pageFontsKey),
            QLatin1String(which == PageFamily::Standard ? standardFamilyKey : fixedFamilyKey),
            family.isEmpty() ? QJsonValue() : QJsonValue(family))) {
        emit pageFontsChanged();
    }
}

void FontSettings::setPageSize(PageSize which, int size)
{
    const auto clamped = size <= 0   ? 0
        : which == PageSize::Default ? std::clamp(size, minimumPageFontSize, maximumPageFontSize)
                                     : std::min(size, maximumPageMinimumFontSize);
    if (storedSize(which) == clamped) {
        return;
    }
    if (!m_settings.setMember(QLatin1String(pageFontsKey),
            QLatin1String(which == PageSize::Default ? pageSizeKey : pageMinimumSizeKey),
            clamped > 0 ? QJsonValue(clamped) : QJsonValue())) {
        emit pageFontsChanged();
    }
}

void FontSettings::setEngineFonts(const PageFonts &fonts)
{
    m_engineFonts = fonts;
    emit pageFontsChanged();
}

QStringList FontSettings::installedFamilies() const { return m_installedFamilies; }

const QString &FontSettings::storedFamily(PageFamily which) const
{
    return which == PageFamily::Standard ? m_pageFonts.standardFamily : m_pageFonts.fixedFamily;
}

int FontSettings::storedSize(PageSize which) const
{
    return which == PageSize::Default ? m_pageFonts.fontSize : m_pageFonts.minimumFontSize;
}

QString FontSettings::installedFamily(const QString &family) const
{
    return m_installedFamilies.contains(family) ? family : QString {};
}

// A value settings.json cannot vouch for sets nothing: only a value in the
// shape this writes is a value the reader chose.
void FontSettings::load()
{
    m_interfaceFontSize.reset();
    m_pageFonts = {};
    const auto interfaceSize = m_settings.value(QLatin1String(interfaceSizeKey));
    if (interfaceSize.isDouble()) {
        m_interfaceFontSize
            = std::clamp(interfaceSize.toInt(), minimumInterfaceFontSize, maximumInterfaceFontSize);
    }
    const auto page = m_settings.value(QLatin1String(pageFontsKey)).toObject();
    m_pageFonts.standardFamily = page.value(QLatin1String(standardFamilyKey)).toString();
    m_pageFonts.fixedFamily = page.value(QLatin1String(fixedFamilyKey)).toString();
    m_pageFonts.fontSize = sizeOrZero(page.value(QLatin1String(pageSizeKey)));
    m_pageFonts.minimumFontSize = sizeOrZero(page.value(QLatin1String(pageMinimumSizeKey)));
}

void registerFontSettings()
{
    qmlRegisterUncreatableType<FontSettings>("Omaweb", 1, 0, "FontSettings",
        QStringLiteral("The reader's font settings are handed to QML, not built there."));
}

} // namespace omaweb
