#include "CssHighlighter.h"

#include <QTest>
#include <QTextBlock>
#include <QTextDocument>
#include <QTextLayout>

using omaweb::CssHighlighter;

class CssHighlighterTest final : public QObject {
    Q_OBJECT

private slots:
    void coloursEachPartOfARule();
    void carriesACommentAcrossLines();
};

namespace {

const QColor comment(QStringLiteral("#808080"));
const QColor selector(QStringLiteral("#7c6cff"));
const QColor propertyName(QStringLiteral("#d8dbe3"));
const QColor value(QStringLiteral("#f38ba8"));

void colour(CssHighlighter &highlighter)
{
    highlighter.setCommentColor(comment);
    highlighter.setSelectorColor(selector);
    highlighter.setPropertyColor(propertyName);
    highlighter.setValueColor(value);
}

// The colour drawn at a character of the document, or none.
QColor colourAt(const QTextDocument &document, int position)
{
    const auto block = document.findBlock(position);
    const auto offset = position - block.position();
    for (const auto &range : block.layout()->formats()) {
        if (offset >= range.start && offset < range.start + range.length) {
            return range.format.foreground().color();
        }
    }
    return {};
}

int at(const QTextDocument &document, const QString &text)
{
    return static_cast<int>(document.toPlainText().indexOf(text));
}

} // namespace

void CssHighlighterTest::coloursEachPartOfARule()
{
    QTextDocument document(QStringLiteral("/* quieter */\n"
                                          "nav a:hover, #top > .promo {\n"
                                          "  text-decoration: none !important;\n"
                                          "}\n"
                                          "/* Hidden by Omaweb */ .ad { display: none; }"));
    CssHighlighter highlighter;
    colour(highlighter);
    highlighter.setDocument(&document);
    highlighter.rehighlight();

    QCOMPARE(colourAt(document, at(document, "quieter")), comment);
    QCOMPARE(colourAt(document, at(document, "a:hover")), selector);
    QCOMPARE(colourAt(document, at(document, ".promo")), selector);
    QCOMPARE(colourAt(document, at(document, "text-decoration")), propertyName);
    QCOMPARE(colourAt(document, at(document, "none !important")), value);
    QCOMPARE(colourAt(document, at(document, "Hidden by")), comment);
    QCOMPARE(colourAt(document, at(document, ".ad")), selector);
    QCOMPARE(colourAt(document, at(document, "display")), propertyName);
    QCOMPARE(colourAt(document, at(document, "none; }")), value);
}

// A comment left open runs on until it is closed, however many lines that
// takes, and what follows it is CSS again.
void CssHighlighterTest::carriesACommentAcrossLines()
{
    QTextDocument document(QStringLiteral("/* first\n"
                                          "a { color: red; }\n"
                                          "still */ p { margin: 0; }"));
    CssHighlighter highlighter;
    colour(highlighter);
    highlighter.setDocument(&document);
    highlighter.rehighlight();

    QCOMPARE(colourAt(document, at(document, "color: red")), comment);
    QCOMPARE(colourAt(document, at(document, "still")), comment);
    QCOMPARE(colourAt(document, at(document, "p {")), selector);
    QCOMPARE(colourAt(document, at(document, "margin")), propertyName);
}

QTEST_MAIN(CssHighlighterTest)
#include "tst_csshighlighter.moc"
