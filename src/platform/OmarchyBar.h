#pragma once

#include <QByteArray>
#include <QObject>
#include <QString>
#include <QStringList>

#include <deque>
#include <functional>

namespace omaweb {

// The id Omaweb's bar widget goes by in Omarchy's plugin list and its bar.
inline constexpr auto omarchyBarWidgetId = "omaweb.bar";

// Every file placing the widget touches, passed in so a test can point them all
// at a temporary home.
struct OmarchyBarPaths {
    // `~/.config/omarchy/plugins`, where the shell finds a plugin by its
    // directory.
    QString plugins;
    // `~/.config/omarchy/shell.json`, the bar's layout once the reader has one.
    QString shellConfig;
    // The layout the shell uses while the reader has none of their own,
    // `$OMARCHY_PATH/config/omarchy/shell.json`.
    QString defaultShellConfig;

    static OmarchyBarPaths fromEnvironment();

    QString link() const;
};

// Runs a program, hands it `input` and answers with its exit code and what it
// printed, or with -1 where it could not be run. A test answers in its place.
using OmarchyCommand = std::function<void(const QString &program, const QStringList &arguments,
    const QByteArray &input, std::function<void(int exitCode, const QByteArray &output)> done)>;

// The command that runs a real program, without waiting on it.
OmarchyCommand runOmarchyCommand();

// Puts the widget the package ships into the reader's Omarchy bar and takes it
// out again (#906). Placing links the widget into the shell's plugins, so it
// is the package's copy that runs and an upgrade reaches it, and puts it in the
// layout twice: the tabs after the workspaces, and the location first in the
// center. Removing takes out both and the link.
//
// The layout is edited with `jq`, as Omarchy's own commands edit it, so the
// reader's file keeps its order and its indentation. The shell is asked to
// read it again after each edit.
class OmarchyBarPlacement final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool available READ available NOTIFY availableChanged)
    Q_PROPERTY(QString spaceNames READ spaceNames NOTIFY spaceNamesChanged)

public:
    OmarchyBarPlacement(OmarchyBarPaths paths, QString shippedWidget, OmarchyCommand command,
        QObject *parent = nullptr);

    // Whether Omarchy's shell answered the last check. The setting is offered
    // only where it did.
    bool available() const;
    void checkAvailable();

    void place();
    void remove();
    // Points the link at the widget this build ships, as each start does
    // while the bar shows the tabs, and leaves the layout alone: where the
    // reader moved the widget is theirs.
    void relink();

    // How the tabs' placement names a Space: "full", or "letter" for its first
    // letter. It is the widget's own setting in the layout, so Omaweb's
    // Settings and the bar's agree.
    QString spaceNames() const;
    void setSpaceNames(const QString &spaceNames);
    void readSpaceNames();

signals:
    void availableChanged();
    void spaceNamesChanged();

private:
    // A change, which calls `next` once it is over.
    using Step = std::function<void(const std::function<void()> &next)>;
    void enqueue(Step step);
    void runNextStep();
    bool link();
    void editLayout(const QString &filter, std::function<void()> then);
    void shell(const QString &method);

    OmarchyBarPaths m_paths;
    QString m_shippedWidget;
    OmarchyCommand m_command;
    bool m_available = false;
    QString m_spaceNames = QStringLiteral("full");
    std::deque<Step> m_steps;
    bool m_stepping = false;
};

} // namespace omaweb
