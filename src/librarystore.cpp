#include "librarystore.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QSet>
#include <QUuid>
#include <algorithm>

namespace {
QJsonObject bookToJson(const Book &book)
{
    QJsonObject object;
    object.insert("id", book.id);
    object.insert("title", book.title);
    object.insert("file", book.storedFileName);
    object.insert("position", book.position);
    object.insert("characterCount", book.characterCount);
    object.insert("lastRead", book.lastRead.toString(Qt::ISODate));
    QJsonArray titles;
    for (const QString &title : book.chapterTitles) titles.append(title);
    object.insert("chapterTitles", titles);
    QJsonArray offsets;
    for (int offset : book.chapterOffsets) offsets.append(offset);
    object.insert("chapterOffsets", offsets);
    QJsonArray bookmarks;
    for (const Bookmark &bookmark : book.bookmarks) {
        bookmarks.append(QJsonObject{{"id", bookmark.id}, {"position", bookmark.position},
                                     {"label", bookmark.label},
                                     {"createdAt", bookmark.createdAt.toString(Qt::ISODate)}});
    }
    object.insert("bookmarks", bookmarks);
    QJsonArray annotations;
    for (const Annotation &annotation : book.annotations) {
        annotations.append(QJsonObject{{"id", annotation.id}, {"start", annotation.start},
                                       {"end", annotation.end}, {"quote", annotation.quote},
                                       {"note", annotation.note},
                                       {"createdAt", annotation.createdAt.toString(Qt::ISODate)}});
    }
    object.insert("annotations", annotations);
    return object;
}

Book bookFromJson(const QJsonObject &object)
{
    Book book;
    book.id = object.value("id").toString();
    book.title = object.value("title").toString();
    book.storedFileName = QFileInfo(object.value("file").toString()).fileName();
    book.position = qMax(0, object.value("position").toInt());
    book.characterCount = qMax(0, object.value("characterCount").toInt());
    book.lastRead = QDateTime::fromString(object.value("lastRead").toString(), Qt::ISODate);
    for (const auto value : object.value("chapterTitles").toArray()) book.chapterTitles.append(value.toString());
    for (const auto value : object.value("chapterOffsets").toArray()) book.chapterOffsets.append(qMax(0, value.toInt()));
    for (const auto value : object.value("bookmarks").toArray()) {
        const auto entry = value.toObject();
        book.bookmarks.append({entry.value("id").toString(), qMax(0, entry.value("position").toInt()),
                               entry.value("label").toString(),
                               QDateTime::fromString(entry.value("createdAt").toString(), Qt::ISODate)});
    }
    for (const auto value : object.value("annotations").toArray()) {
        const auto entry = value.toObject();
        book.annotations.append({entry.value("id").toString(), qMax(0, entry.value("start").toInt()),
                                 qMax(0, entry.value("end").toInt()), entry.value("quote").toString(),
                                 entry.value("note").toString(),
                                 QDateTime::fromString(entry.value("createdAt").toString(), Qt::ISODate)});
    }
    return book;
}

QList<Book> readLibrary(const QString &path, QString *error)
{
    QFile file(path);
    if (!file.exists()) return {};
    if (!file.open(QIODevice::ReadOnly)) {
        if (error) *error = file.errorString();
        return {};
    }
    QJsonParseError parseError;
    const auto document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isArray()) {
        if (error) *error = QStringLiteral("书架数据无法读取：%1").arg(parseError.errorString());
        return {};
    }
    QList<Book> books;
    for (const auto value : document.array()) {
        Book book = bookFromJson(value.toObject());
        if (!book.id.isEmpty() && !book.storedFileName.isEmpty()) books.append(book);
    }
    return books;
}

bool writeLibrary(const QString &path, const QList<Book> &books, QString *error)
{
    QJsonArray array;
    for (const Book &book : books) array.append(bookToJson(book));
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly) || file.write(QJsonDocument(array).toJson()) < 0 || !file.commit()) {
        if (error) *error = file.errorString();
        return false;
    }
    return true;
}

bool copyReplacing(const QString &source, const QString &target, QString *error, bool force = false)
{
    if (!QFileInfo::exists(source)) {
        if (error) *error = QStringLiteral("同步所需的书籍文件不存在：%1").arg(source);
        return false;
    }
    if (QFileInfo(source).absoluteFilePath() == QFileInfo(target).absoluteFilePath()) return true;
    if (!force && QFileInfo::exists(target) && QFileInfo(source).lastModified() <= QFileInfo(target).lastModified()) return true;
    if (QFileInfo::exists(target) && !QFile::remove(target)) {
        if (error) *error = QStringLiteral("无法更新同步目录中的文件：%1").arg(target);
        return false;
    }
    QDir().mkpath(QFileInfo(target).path());
    if (!QFile::copy(source, target)) {
        if (error) *error = QStringLiteral("无法复制同步文件：%1").arg(target);
        return false;
    }
    return true;
}

void mergeEntries(QList<Bookmark> &target, const QList<Bookmark> &source)
{
    QSet<QString> ids;
    for (const auto &entry : target) ids.insert(entry.id);
    for (const auto &entry : source) if (!ids.contains(entry.id)) target.append(entry);
}

void mergeEntries(QList<Annotation> &target, const QList<Annotation> &source)
{
    QSet<QString> ids;
    for (const auto &entry : target) ids.insert(entry.id);
    for (const auto &entry : source) if (!ids.contains(entry.id)) target.append(entry);
}
}

LibraryStore::LibraryStore(QString dataDirectory) : m_dataDirectory(std::move(dataDirectory))
{
    QDir().mkpath(m_dataDirectory);
    load();
}

QList<Book> LibraryStore::books() const
{
    QList<Book> sorted = m_books;
    std::stable_sort(sorted.begin(), sorted.end(), [](const Book &left, const Book &right) {
        if (left.lastRead.isValid() != right.lastRead.isValid()) return left.lastRead.isValid();
        return left.lastRead > right.lastRead;
    });
    return sorted;
}

bool LibraryStore::load(QString *error)
{
    QString loadError;
    m_books = readLibrary(QDir(m_dataDirectory).filePath("library.json"), &loadError);
    if (error) *error = loadError;
    return loadError.isEmpty();
}

bool LibraryStore::save(QString *error) const
{
    return writeLibrary(QDir(m_dataDirectory).filePath("library.json"), m_books, error);
}

QString LibraryStore::importTextFile(const QString &sourcePath, QString *error)
{
    QFile source(sourcePath);
    if (!source.open(QIODevice::ReadOnly)) {
        if (error) *error = source.errorString();
        return {};
    }
    const QByteArray contents = source.readAll();
    if (contents.isEmpty()) {
        if (error) *error = QStringLiteral("文件为空，无法导入。");
        return {};
    }
    const QString decoded = QString::fromUtf8(contents.constData(), contents.size());
    if (decoded.toUtf8() != contents) {
        if (error) *error = QStringLiteral("文件不是有效的 UTF-8 文本，暂不支持导入。");
        return {};
    }
    const QString id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    const QString storedName = id + ".txt";
    QFile copy(QDir(m_dataDirectory).filePath(storedName));
    if (!copy.open(QIODevice::WriteOnly) || copy.write(contents) != contents.size()) {
        if (error) *error = copy.errorString();
        return {};
    }
    copy.close();
    Book book{id, QFileInfo(sourcePath).completeBaseName(), storedName, 0,
              static_cast<int>(decoded.size()), {}};
    m_books.prepend(book);
    if (!save(error)) {
        m_books.removeFirst();
        QFile::remove(copy.fileName());
        return {};
    }
    return id;
}

QString LibraryStore::importEpubFile(const QString &sourcePath, QString *error)
{
    EpubDocument document;
    if (!EpubDocument::load(sourcePath, &document, error)) return {};
    QFile source(sourcePath);
    if (!source.open(QIODevice::ReadOnly)) {
        if (error) *error = source.errorString();
        return {};
    }
    const QByteArray contents = source.readAll();
    const QString id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    const QString storedName = id + ".epub";
    QFile copy(QDir(m_dataDirectory).filePath(storedName));
    if (!copy.open(QIODevice::WriteOnly) || copy.write(contents) != contents.size()) {
        if (error) *error = copy.errorString();
        return {};
    }
    copy.close();
    Book book;
    book.id = id;
    book.title = document.title;
    book.storedFileName = storedName;
    book.characterCount = document.text.size();
    for (const auto &chapter : document.chapters) {
        book.chapterTitles.append(chapter.title);
        book.chapterOffsets.append(chapter.offset);
    }
    m_books.prepend(book);
    if (!save(error)) {
        m_books.removeFirst();
        QFile::remove(copy.fileName());
        return {};
    }
    return id;
}

QString LibraryStore::textPath(const Book &book) const
{
    return QDir(m_dataDirectory).filePath(book.storedFileName);
}

bool LibraryStore::savePosition(const QString &bookId, int position, QString *error)
{
    for (Book &book : m_books) {
        if (book.id == bookId) {
            book.position = qMax(0, position);
            book.lastRead = QDateTime::currentDateTime();
            return save(error);
        }
    }
    if (error) *error = QStringLiteral("找不到这本书。");
    return false;
}

bool LibraryStore::addBookmark(const QString &bookId, int position, QString *error)
{
    for (Book &book : m_books) {
        if (book.id != bookId) continue;
        book.bookmarks.append({QUuid::createUuid().toString(QUuid::WithoutBraces), qMax(0, position),
                               QStringLiteral("书签 %1").arg(book.bookmarks.size() + 1),
                               QDateTime::currentDateTime()});
        return save(error);
    }
    if (error) *error = QStringLiteral("找不到这本书。");
    return false;
}

bool LibraryStore::addAnnotation(const QString &bookId, int start, int end, const QString &quote,
                                 const QString &note, QString *error)
{
    for (Book &book : m_books) {
        if (book.id != bookId) continue;
        if (start < 0 || end <= start || quote.isEmpty()) {
            if (error) *error = QStringLiteral("请先选择需要划线的文本。");
            return false;
        }
        book.annotations.append({QUuid::createUuid().toString(QUuid::WithoutBraces), start, end,
                                 quote, note, QDateTime::currentDateTime()});
        return save(error);
    }
    if (error) *error = QStringLiteral("找不到这本书。");
    return false;
}

bool LibraryStore::syncFolder(const QString &folderPath, QString *error)
{
    if (folderPath.isEmpty() || !QDir().mkpath(QDir(folderPath).filePath("books"))) {
        if (error) *error = QStringLiteral("无法创建同步文件夹。");
        return false;
    }
    const QString remoteLibrary = QDir(folderPath).filePath("library.json");
    QString remoteError;
    QList<Book> remoteBooks = readLibrary(remoteLibrary, &remoteError);
    if (!remoteError.isEmpty()) {
        if (error) *error = remoteError;
        return false;
    }
    QHash<QString, int> mergedIndex;
    QList<Book> merged = m_books;
    for (int i = 0; i < merged.size(); ++i) mergedIndex.insert(merged.at(i).id, i);
    QDir remoteFiles(QDir(folderPath).filePath("books"));
    for (const Book &remote : remoteBooks) {
        const QString localPath = QDir(m_dataDirectory).filePath(remote.storedFileName);
        const QString remotePath = remoteFiles.filePath(remote.storedFileName);
        if (!mergedIndex.contains(remote.id)) {
            Book received = remote;
            if (!copyReplacing(remotePath, localPath, error)) return false;
            mergedIndex.insert(received.id, merged.size());
            merged.append(received);
            continue;
        }
        Book &local = merged[mergedIndex.value(remote.id)];
        const bool remoteNewer = remote.lastRead.isValid() &&
            (!local.lastRead.isValid() || remote.lastRead > local.lastRead);
        Book selected = remoteNewer ? remote : local;
        mergeEntries(selected.bookmarks, remote.bookmarks);
        mergeEntries(selected.bookmarks, local.bookmarks);
        mergeEntries(selected.annotations, remote.annotations);
        mergeEntries(selected.annotations, local.annotations);
        const QString source = remoteNewer ? remotePath : localPath;
        if (!copyReplacing(source, localPath, error, remoteNewer) ||
            !copyReplacing(source, remotePath, error, !remoteNewer)) return false;
        local = selected;
    }
    for (const Book &book : merged) {
        const QString localPath = QDir(m_dataDirectory).filePath(book.storedFileName);
        const QString remotePath = remoteFiles.filePath(book.storedFileName);
        if (!copyReplacing(localPath, remotePath, error)) return false;
    }
    const QList<Book> previousBooks = m_books;
    m_books = merged;
    if (!save(error)) {
        m_books = previousBooks;
        return false;
    }
    return writeLibrary(remoteLibrary, m_books, error);
}
