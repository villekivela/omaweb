#include "FileChooser.h"

#include <QQmlEngine>

namespace omaweb {

FileChooser::FileChooser(QObject *parent)
    : QObject(parent)
{
}

FileChooser::~FileChooser() { close(); }

bool FileChooser::isOpen() const { return !m_handle.isEmpty(); }

void FileChooser::forget()
{
    if (m_handle.isEmpty()) {
        return;
    }
    m_handle.clear();
    emit openChanged();
}

void registerFileChooser() { qmlRegisterType<FileChooser>("Omaweb", 1, 0, "FileChooser"); }

} // namespace omaweb
