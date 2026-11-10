#pragma once

#include <QColor>
#include <QPointer>
#include <QQuickTextDocument>
#include <QSyntaxHighlighter>

namespace omaweb {

// Colours the CSS in the Site style panel's editor: comments, selectors, the
// properties a rule sets and their values, in the colours the panel gives it
// from the theme. It reads text, not a grammar: a rule broken across lines or
// left half-typed is coloured by where its braces and colons stand.
class CssHighlighter : public QSyntaxHighlighter {
    Q_OBJECT
    Q_PROPERTY(QQuickTextDocument *textDocument READ textDocument WRITE setTextDocument NOTIFY
            textDocumentChanged)
    Q_PROPERTY(QColor commentColor READ commentColor WRITE setCommentColor NOTIFY coloursChanged)
    Q_PROPERTY(QColor selectorColor READ selectorColor WRITE setSelectorColor NOTIFY coloursChanged)
    Q_PROPERTY(QColor propertyColor READ propertyColor WRITE setPropertyColor NOTIFY coloursChanged)
    Q_PROPERTY(QColor valueColor READ valueColor WRITE setValueColor NOTIFY coloursChanged)

public:
    explicit CssHighlighter(QObject *parent = nullptr);

    // The editor's document, as a TextArea hands it over.
    QQuickTextDocument *textDocument() const;
    void setTextDocument(QQuickTextDocument *document);

    QColor commentColor() const;
    void setCommentColor(const QColor &colour);
    QColor selectorColor() const;
    void setSelectorColor(const QColor &colour);
    QColor propertyColor() const;
    void setPropertyColor(const QColor &colour);
    QColor valueColor() const;
    void setValueColor(const QColor &colour);

signals:
    void textDocumentChanged();
    void coloursChanged();

protected:
    void highlightBlock(const QString &text) override;

private:
    void setColour(QColor &colour, const QColor &value);

    QPointer<QQuickTextDocument> m_textDocument;
    QColor m_comment;
    QColor m_selector;
    QColor m_property;
    QColor m_value;
};

// Makes `CssHighlighter` available to QML as `import Omaweb`.
void registerCssHighlighter();

} // namespace omaweb
