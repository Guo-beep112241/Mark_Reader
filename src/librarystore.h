#pragma once

#include "book.h"
#include "epubdocument.h"

#include <QByteArray>
#include <QList>
#include <QString>
#include <QStringList>

// 从本地文件夹导入后的统计，用于向用户说明这次同步做了什么。
struct FolderImportSummary {
    int imported = 0;
    int skippedDuplicates = 0;
    int removedDuplicates = 0;
    int failed = 0;
    QStringList warnings;
};

class LibraryStore {
public:
    explicit LibraryStore(QString dataDirectory);

    QList<Book> books() const;
    QString importTextFile(const QString &sourcePath, QString *error = nullptr);
    QString importEpubFile(const QString &sourcePath, QString *error = nullptr);
    // 扫描文件夹（含子文件夹）中的 TXT/EPUB，把书库里还没有的书导入进来。
    // 书名与正文高度重复的书只会保留一本；重复副本会被跳过或合并。
    bool importFolder(const QString &folderPath, FolderImportSummary *summary = nullptr,
                      QString *error = nullptr);
    // 合并书库中书名与正文高度重复的书，返回被合并掉的本数；失败返回 -1。
    int dedupe(QString *error = nullptr);
    // 默认同步来源：iCloud Drive 的“本机迁移/Documents/kindle”目录。
    static QString defaultSyncFolder();
    QString textPath(const Book &book) const;
    // 每本书的封面由书籍 ID 派生，文件名固定，便于同步文件夹一起复制。
    QString coverPath(const Book &book) const;
    bool savePosition(const QString &bookId, int position, QString *error = nullptr);
    bool addBookmark(const QString &bookId, int position, QString *error = nullptr);
    bool addAnnotation(const QString &bookId, int start, int end, const QString &quote,
                       const QString &note, QString *error = nullptr);

private:
    bool load(QString *error = nullptr);
    bool save(QString *error = nullptr) const;
    bool ensureCovers();
    bool writeCover(const Book &book, const QByteArray &preferredCover, QString *error = nullptr);
    void removeCover(const Book &book);
    QString insertBook(Book book, const QString &suffix, const QByteArray &contents,
                       const QByteArray &coverData, QString *error);
    bool mergeDuplicates(int *removed, QString *error);
    QString m_dataDirectory;
    QList<Book> m_books;
};
