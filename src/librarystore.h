#pragma once

#include "book.h"
#include "epubdocument.h"

#include <QList>
#include <QString>

class LibraryStore {
public:
    explicit LibraryStore(QString dataDirectory);

    QList<Book> books() const;
    QString importTextFile(const QString &sourcePath, QString *error = nullptr);
    QString importEpubFile(const QString &sourcePath, QString *error = nullptr);
    QString textPath(const Book &book) const;
    bool savePosition(const QString &bookId, int position, QString *error = nullptr);
    bool addBookmark(const QString &bookId, int position, QString *error = nullptr);
    bool addAnnotation(const QString &bookId, int start, int end, const QString &quote,
                       const QString &note, QString *error = nullptr);
    bool syncFolder(const QString &folderPath, QString *error = nullptr);

private:
    bool load(QString *error = nullptr);
    bool save(QString *error = nullptr) const;
    QString m_dataDirectory;
    QList<Book> m_books;
};
