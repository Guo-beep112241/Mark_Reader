#pragma once

#include "book.h"
#include "epubdocument.h"

#include <QByteArray>
#include <QList>
#include <QString>

class LibraryStore {
public:
    explicit LibraryStore(QString dataDirectory);

    QList<Book> books() const;
    QString importTextFile(const QString &sourcePath, QString *error = nullptr);
    QString importEpubFile(const QString &sourcePath, QString *error = nullptr);
    QString textPath(const Book &book) const;
    // 每本书的封面由书籍 ID 派生，文件名固定，便于同步文件夹一起复制。
    QString coverPath(const Book &book) const;
    bool savePosition(const QString &bookId, int position, QString *error = nullptr);
    bool addBookmark(const QString &bookId, int position, QString *error = nullptr);
    bool addAnnotation(const QString &bookId, int start, int end, const QString &quote,
                       const QString &note, QString *error = nullptr);
    bool syncFolder(const QString &folderPath, QString *error = nullptr);

private:
    bool load(QString *error = nullptr);
    bool save(QString *error = nullptr) const;
    bool ensureCovers();
    bool writeCover(const Book &book, const QByteArray &preferredCover, QString *error = nullptr);
    void removeCover(const Book &book);
    QString m_dataDirectory;
    QList<Book> m_books;
};
