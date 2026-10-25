#include "librarystore.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QUuid>

LibraryStore::LibraryStore(QString dataDirectory) : m_dataDirectory(std::move(dataDirectory))
{
    QDir().mkpath(m_dataDirectory);
    load();
}

bool LibraryStore::load(QString *error)
{
    QFile file(QDir(m_dataDirectory).filePath("library.json"));
    if (!file.exists()) return true;
    if (!file.open(QIODevice::ReadOnly)) {
        if (error) *error = file.errorString();
        return false;
    }
    QJsonParseError parseError;
    const auto document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isArray()) {
        if (error) *error = QStringLiteral("书架数据无法读取：%1").arg(parseError.errorString());
        return false;
    }
    for (const auto value : document.array()) {
        const auto object = value.toObject();
        Book book;
        book.id = object.value("id").toString();
        book.title = object.value("title").toString();
        book.storedFileName = object.value("file").toString();
        book.position = object.value("position").toInt();
        book.lastRead = QDateTime::fromString(object.value("lastRead").toString(), Qt::ISODate);
        if (!book.id.isEmpty() && !book.storedFileName.isEmpty()) m_books.append(book);
    }
    return true;
}

bool LibraryStore::save(QString *error) const
{
    QJsonArray array;
    for (const Book &book : m_books) {
        QJsonObject object;
        object.insert("id", book.id);
        object.insert("title", book.title);
        object.insert("file", book.storedFileName);
        object.insert("position", book.position);
        object.insert("lastRead", book.lastRead.toString(Qt::ISODate));
        array.append(object);
    }
    QSaveFile file(QDir(m_dataDirectory).filePath("library.json"));
    if (!file.open(QIODevice::WriteOnly) || file.write(QJsonDocument(array).toJson()) < 0 || !file.commit()) {
        if (error) *error = file.errorString();
        return false;
    }
    return true;
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
    Book book{id, QFileInfo(sourcePath).completeBaseName(), storedName, 0, QDateTime::currentDateTime()};
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
