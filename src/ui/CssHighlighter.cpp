#include "CssHighlighter.h"

#include <QQuickTextDocument>
#include <QtQml>

namespace omaweb {

CssHighlighter::CssHighlighter(QObject *parent)
    : QSyntaxHighlighter(parent)
{
}

QQuickTextDocument *CssHighlighter::textDocument() const { return m_textDocument; }

void CssHighlighter::setTextDocument(QQuickTextDocument *document)
{
    if (document == m_textDocument) {
        return;
    }
    m_textDocument = document;
    setDocument(document ? document->textDocument() : nullptr);
    emit textDocumentChanged();
}

QColor CssHighlighter::commentColor() const { return m_comment; }
void CssHighlighter::setCommentColor(const QColor &colour) { setColour(m_comment, colour); }
QColor CssHighlighter::selectorColor() const { return m_selector; }
void CssHighlighter::setSelectorColor(const QColor &colour) { setColour(m_selector, colour); }
QColor CssHighlighter::propertyColor() const { return m_property; }
void CssHighlighter::setPropertyColor(const QColor &colour) { setColour(m_property, colour); }
QColor CssHighlighter::valueColor() const { return m_value; }
void CssHighlighter::setValueColor(const QColor &colour) { setColour(m_value, colour); }

void CssHighlighter::setColour(QColor &colour, const QColor &value)
{
    if (colour == value) {
        return;
    }
    colour = value;
    emit coloursChanged();
    rehighlight();
}

namespace {

    // Where a line ends, carried to the next as its block state.
    enum State : int {
        InComment = 1,
        InRule = 2,
        InValue = 4,
    };

} // namespace

void CssHighlighter::highlightBlock(const QString &text)
{
    const auto previous = previousBlockState();
    int state = previous < 0 ? 0 : previous;
    const auto size = static_cast<int>(text.size());
    int index = 0;
    while (index < size) {
        if (state & InComment) {
            const auto end = text.indexOf(QLatin1String("*/"), index);
            const auto stop = end < 0 ? size : static_cast<int>(end) + 2;
            setFormat(index, stop - index, m_comment);
            index = stop;
            if (end >= 0) {
                state &= ~InComment;
            }
            continue;
        }
        const auto character = text.at(index);
        if (text.mid(index, 2) == QLatin1String("/*")) {
            state |= InComment;
            continue;
        }
        if (!(state & InRule)) {
            if (character == u'{') {
                state |= InRule;
            } else if (!character.isSpace()) {
                setFormat(index, 1, m_selector);
            }
        } else if (character == u'}') {
            state &= ~(InRule | InValue);
        } else if (state & InValue) {
            if (character == u';') {
                state &= ~InValue;
            } else if (!character.isSpace()) {
                setFormat(index, 1, m_value);
            }
        } else if (character == u':') {
            state |= InValue;
        } else if (!character.isSpace() && character != u';') {
            setFormat(index, 1, m_property);
        }
        ++index;
    }
    setCurrentBlockState(state);
}

void registerCssHighlighter() { qmlRegisterType<CssHighlighter>("Omaweb", 1, 0, "CssHighlighter"); }

} // namespace omaweb
