#include "KeyEventLog.h"

#include <QGuiApplication>
#include <QKeyEvent>
#include <QKeySequence>
#include <QLoggingCategory>
#include <QWindow>

namespace omaweb {

namespace {

    Q_LOGGING_CATEGORY(keyLog, "omaweb.keys", QtWarningMsg)

    QString describe(const QObject *object)
    {
        if (!object)
            return QStringLiteral("none");
        return QStringLiteral(R"(%1 "%2")")
            .arg(QLatin1StringView(object->metaObject()->className()), object->objectName());
    }

    // A control character is what a chord types, and written raw it would break the line.
    QString escaped(const QString &text)
    {
        QString written;
        for (const auto character : text) {
            if (character.isPrint() && character != u'"' && character != u'\\')
                written += character;
            else
                written += QStringLiteral("\\x%1").arg(
                    uint(character.unicode()), 2, 16, QLatin1Char('0'));
        }
        return written;
    }

    class KeyEventLog final : public QObject {
    public:
        explicit KeyEventLog(QGuiApplication *application)
            : QObject(application)
        {
            connect(application, &QGuiApplication::focusObjectChanged, this, [](QObject *focus) {
                qCInfo(keyLog).noquote() << "focus moved to" << describe(focus);
            });
            application->installEventFilter(this);
        }

    protected:
        bool eventFilter(QObject *receiver, QEvent *event) override
        {
            QString key;
            QString text;
            switch (event->type()) {
            case QEvent::KeyPress:
            case QEvent::ShortcutOverride: {
                const auto *press = static_cast<QKeyEvent *>(event);
                key = QStringLiteral("%1 %2 text \"%3\"")
                          .arg(event->type() == QEvent::KeyPress ? QStringLiteral("press")
                                                                 : QStringLiteral("override"),
                              QKeySequence(press->keyCombination())
                                  .toString(QKeySequence::PortableText),
                              escaped(press->text()));
                break;
            }
            case QEvent::Shortcut:
                key = QStringLiteral("shortcut %1")
                          .arg(static_cast<QShortcutEvent *>(event)->key().toString(
                              QKeySequence::PortableText));
                break;
            default:
                return false;
            }
            // The shell's dialog is a property of its window, so the window a key was for says
            // whether the new-Space dialog was open to take it.
            auto *window = qobject_cast<QWindow *>(receiver);
            if (!window)
                window = QGuiApplication::focusWindow();
            const auto dialog = window ? window->property("dialogMode") : QVariant();
            qCInfo(keyLog).noquote()
                << QStringLiteral("%1 to %2, focus %3, dialog %4")
                       .arg(key, describe(receiver), describe(QGuiApplication::focusObject()),
                           dialog.isValid()
                               ? QStringLiteral(R"("%1")").arg(escaped(dialog.toString()))
                               : QStringLiteral("none"));
            return false;
        }
    };

} // namespace

QObject *installKeyEventLog(QGuiApplication *application)
{
    if (!keyLog().isInfoEnabled())
        return nullptr;
    return new KeyEventLog(application);
}

} // namespace omaweb
