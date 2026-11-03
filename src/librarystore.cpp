#include "librarystore.h"

#include "coverart.h"

#include <QCryptographicHash>
#include <QDir>
#include <QDirIterator>
#include <QImage>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
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

// 判断两本书是否重复的门槛：书名要足够接近，正文也要大量重合。
constexpr double kTitleSimilarityThreshold = 0.82;
constexpr double kContentSimilarityThreshold = 0.85;

struct ContentSignature {
    QByteArray digest;
    QSet<quint64> shingles;
    bool usable = false;
};

// 去掉“（1）”“副本”“copy”这类复制文件名后缀，避免同一本书因重复下载被判成不同书。
QString withoutCopySuffix(QString title)
{
    static const QRegularExpression numbered(QStringLiteral("\\s*[（(]\\s*\\d+\\s*[)）]\\s*$"));
    static const QRegularExpression bracketed(
        QStringLiteral("\\s*[（(]\\s*(?:副本|拷贝|copy)\\s*[)）]\\s*$"),
        QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression separated(
        QStringLiteral("\\s*[-_—·]\\s*(?:副本|拷贝|copy)\\s*$"),
        QRegularExpression::CaseInsensitiveOption);
    title.remove(numbered);
    title.remove(bracketed);
    title.remove(separated);
    return title.trimmed();
}

QString normalizedTitle(const QString &title)
{
    const QString folded =
        withoutCopySuffix(title).normalized(QString::NormalizationForm_KC).toCaseFolded();
    QString key;
    key.reserve(folded.size());
    for (const QChar character : folded)
        if (character.isLetterOrNumber()) key.append(character);
    return key;
}

// 用字符二元组的 Dice 系数衡量书名相似度，能容忍标点、副标题和复制后缀的差异。
// 归一化后的书名可以缓存，比较时直接传入，避免每次两两比较都重新计算。
double normalizedTitleSimilarity(const QString &first, const QString &second)
{
    if (first.isEmpty() || second.isEmpty()) return 0.0;
    if (first == second) return 1.0;
    if (first.size() < 2 || second.size() < 2) return 0.0;
    QSet<QString> firstPairs;
    QSet<QString> secondPairs;
    for (int i = 0; i + 1 < first.size(); ++i) firstPairs.insert(first.mid(i, 2));
    for (int i = 0; i + 1 < second.size(); ++i) secondPairs.insert(second.mid(i, 2));
    int shared = 0;
    for (const QString &pair : firstPairs)
        if (secondPairs.contains(pair)) ++shared;
    return 2.0 * shared / (firstPairs.size() + secondPairs.size());
}

// 正文去掉空白后取整段 SHA-256，再按内容本身采样若干 32 字片段。
// 整段相同说明是完全相同的副本；片段大量重合说明只是前后多了一点内容，
// 用内容决定采样位置可以避免插入几行文字后所有特征全部错位。
ContentSignature signatureForText(const QString &text)
{
    ContentSignature signature;
    const QString normalized = text.normalized(QString::NormalizationForm_KC);
    QString compact;
    compact.reserve(normalized.size());
    for (const QChar character : normalized)
        if (!character.isSpace()) compact.append(character);
    if (compact.isEmpty()) return signature;
    signature.digest = QCryptographicHash::hash(compact.toUtf8(), QCryptographicHash::Sha256);
    constexpr int kShingleLength = 32;
    constexpr quint64 kShingleBase = 1315423911ull;
    if (compact.size() <= kShingleLength * 2) {
        signature.shingles.insert(qHash(compact));
    } else {
        quint64 power = 1;
        for (int i = 1; i < kShingleLength; ++i) power *= kShingleBase;
        quint64 rolling = 0;
        for (int i = 0; i < kShingleLength; ++i)
            rolling = rolling * kShingleBase + static_cast<quint64>(compact.at(i).unicode());
        for (int start = 0; start + kShingleLength <= compact.size(); ++start) {
            if (start > 0) {
                rolling = (rolling - static_cast<quint64>(compact.at(start - 1).unicode()) * power)
                        * kShingleBase
                        + static_cast<quint64>(compact.at(start + kShingleLength - 1).unicode());
            }
            const quint64 mixed = rolling * 0x9E3779B97F4A7C15ull;
            if ((mixed >> 58) == 0) signature.shingles.insert(mixed);
        }
        if (signature.shingles.isEmpty()) signature.shingles.insert(qHash(compact));
    }
    signature.usable = true;
    return signature;
}

ContentSignature signatureForBook(const QString &path)
{
    if (path.endsWith(".epub", Qt::CaseInsensitive)) {
        EpubDocument document;
        if (!EpubDocument::load(path, &document, nullptr)) return {};
        return signatureForText(document.text);
    }
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) return {};
    return signatureForText(QString::fromUtf8(file.readAll()));
}

double contentSimilarity(const ContentSignature &left, const ContentSignature &right)
{
    if (!left.usable || !right.usable) return 0.0;
    if (left.digest == right.digest) return 1.0;
    if (left.shingles.isEmpty() || right.shingles.isEmpty()) return 0.0;
    int shared = 0;
    for (const quint64 shingle : left.shingles)
        if (right.shingles.contains(shingle)) ++shared;
    return 2.0 * shared / (left.shingles.size() + right.shingles.size());
}

// 合并重复书籍时，优先留下读得更久、笔记更多的那一本，用户不会丢掉阅读痕迹。
bool keepsBetterReadingTrail(const Book &candidate, const Book &current)
{
    if (candidate.lastRead.isValid() != current.lastRead.isValid()) return candidate.lastRead.isValid();
    if (candidate.lastRead != current.lastRead) return candidate.lastRead > current.lastRead;
    if (candidate.position != current.position) return candidate.position > current.position;
    const int candidateMarks = candidate.bookmarks.size() + candidate.annotations.size();
    const int currentMarks = current.bookmarks.size() + current.annotations.size();
    return candidateMarks > currentMarks;
}

void absorbBook(Book &keeper, const Book &loser, bool identicalContent)
{
    mergeEntries(keeper.bookmarks, loser.bookmarks);
    mergeEntries(keeper.annotations, loser.annotations);
    if (!keeper.lastRead.isValid() || (loser.lastRead.isValid() && loser.lastRead > keeper.lastRead))
        keeper.lastRead = loser.lastRead;
    keeper.characterCount = qMax(keeper.characterCount, loser.characterCount);
    // 正文完全相同时阅读位置可以直接取较靠后的一个；正文有差异时保留 keeper 的位置。
    if (identicalContent) keeper.position = qMax(keeper.position, loser.position);
}
}

LibraryStore::LibraryStore(QString dataDirectory) : m_dataDirectory(std::move(dataDirectory))
{
    QDir().mkpath(m_dataDirectory);
    load();
    ensureCovers();
}

bool LibraryStore::ensureCovers()
{
    bool okay = true;
    for (const Book &book : m_books) {
        const QString path = coverPath(book);
        if (path.isEmpty() || QFileInfo::exists(path)) continue;
        QImage cover;
        if (book.storedFileName.endsWith(".epub", Qt::CaseInsensitive)) {
            EpubDocument document;
            if (EpubDocument::load(textPath(book), &document, nullptr) && !document.coverData.isEmpty())
                cover.loadFromData(document.coverData);
        }
        if (cover.isNull())
            cover = CoverArt::renderFallback(book.title.isEmpty() ? QStringLiteral("无名书") : book.title);
        okay = CoverArt::save(cover, path) && okay;
    }
    return okay;
}

bool LibraryStore::writeCover(const Book &book, const QByteArray &preferredCover, QString *error)
{
    QImage cover;
    if (!preferredCover.isEmpty()) cover.loadFromData(preferredCover);
    if (cover.isNull())
        cover = CoverArt::renderFallback(book.title.isEmpty() ? QStringLiteral("无名书") : book.title);
    if (cover.width() > 1600 || cover.height() > 2200)
        cover = cover.scaled(1200, 1800, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    return CoverArt::save(cover, coverPath(book), error);
}

void LibraryStore::removeCover(const Book &book)
{
    const QString path = coverPath(book);
    if (!path.isEmpty()) QFile::remove(path);
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
    Book book;
    book.title = QFileInfo(sourcePath).completeBaseName();
    book.characterCount = static_cast<int>(decoded.size());
    return insertBook(book, QStringLiteral(".txt"), contents, {}, error);
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
    Book book;
    book.title = document.title.trimmed().isEmpty() ? QFileInfo(sourcePath).completeBaseName()
                                                    : document.title;
    book.characterCount = document.text.size();
    for (const auto &chapter : document.chapters) {
        book.chapterTitles.append(chapter.title);
        book.chapterOffsets.append(chapter.offset);
    }
    return insertBook(book, QStringLiteral(".epub"), contents, document.coverData, error);
}

QString LibraryStore::insertBook(Book book, const QString &suffix, const QByteArray &contents,
                                 const QByteArray &coverData, QString *error)
{
    const QString id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    const QString storedName = id + suffix;
    QFile copy(QDir(m_dataDirectory).filePath(storedName));
    if (!copy.open(QIODevice::WriteOnly) || copy.write(contents) != contents.size()) {
        if (error) *error = copy.errorString();
        return {};
    }
    copy.close();
    book.id = id;
    book.storedFileName = storedName;
    if (!writeCover(book, coverData, error)) {
        QFile::remove(copy.fileName());
        return {};
    }
    m_books.prepend(book);
    if (!save(error)) {
        m_books.removeFirst();
        QFile::remove(copy.fileName());
        removeCover(book);
        return {};
    }
    return id;
}

QString LibraryStore::textPath(const Book &book) const
{
    return QDir(m_dataDirectory).filePath(book.storedFileName);
}

QString LibraryStore::coverPath(const Book &book) const
{
    if (book.id.isEmpty()) return {};
    return QDir(m_dataDirectory).filePath(book.id + "-cover.png");
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

QString LibraryStore::defaultSyncFolder()
{
    return QDir::home().filePath(
        QStringLiteral("Library/Mobile Documents/com~apple~CloudDocs/本机迁移/Documents/kindle"));
}

bool LibraryStore::importFolder(const QString &folderPath, FolderImportSummary *summary, QString *error)
{
    FolderImportSummary collected;
    if (folderPath.isEmpty() || !QDir(folderPath).exists()) {
        if (error) *error = QStringLiteral("同步文件夹不存在：%1").arg(folderPath);
        if (summary) *summary = collected;
        return false;
    }

    QStringList paths;
    QDirIterator iterator(folderPath, QDir::Files | QDir::Readable, QDirIterator::Subdirectories);
    while (iterator.hasNext()) {
        const QString path = iterator.next();
        const QString suffix = QFileInfo(path).suffix().toLower();
        if (suffix != QStringLiteral("txt") && suffix != QStringLiteral("epub")) continue;
        // 旧版“同步文件夹”会把应用内副本导出成 <UUID>.txt/.epub，不要把它们当新书再导入。
        if (!QUuid::fromString(QFileInfo(path).completeBaseName()).isNull()) continue;
        paths.append(path);
    }
    paths.sort();

    // 书库中已有书的正文特征按需计算：只有书名相近时才需要比较正文。
    struct KnownBook {
        Book book;
        QString titleKey;
        ContentSignature signature;
        bool signatureReady = false;
    };
    QList<KnownBook> known;
    known.reserve(m_books.size());
    for (const Book &book : m_books) known.append({book, normalizedTitle(book.title), {}, false});
    const auto knownSignature = [this](KnownBook &entry) -> const ContentSignature & {
        if (!entry.signatureReady) {
            entry.signature = signatureForBook(textPath(entry.book));
            entry.signatureReady = true;
        }
        return entry.signature;
    };

    for (const QString &path : paths) {
        const bool epub = QFileInfo(path).suffix().compare("epub", Qt::CaseInsensitive) == 0;
        const QString fileName = QFileInfo(path).fileName();
        Book book;
        QString candidateText;
        QByteArray contents;
        QByteArray cover;
        if (epub) {
            EpubDocument document;
            QString loadError;
            if (!EpubDocument::load(path, &document, &loadError)) {
                ++collected.failed;
                collected.warnings.append(QStringLiteral("%1：%2").arg(fileName, loadError));
                continue;
            }
            QFile file(path);
            if (!file.open(QIODevice::ReadOnly)) {
                ++collected.failed;
                collected.warnings.append(QStringLiteral("%1：%2").arg(fileName, file.errorString()));
                continue;
            }
            contents = file.readAll();
            book.title = document.title.trimmed().isEmpty() ? QFileInfo(path).completeBaseName()
                                                            : document.title;
            book.characterCount = static_cast<int>(document.text.size());
            for (const EpubChapter &chapter : document.chapters) {
                book.chapterTitles.append(chapter.title);
                book.chapterOffsets.append(chapter.offset);
            }
            cover = document.coverData;
            candidateText = document.text;
        } else {
            QFile file(path);
            if (!file.open(QIODevice::ReadOnly)) {
                ++collected.failed;
                collected.warnings.append(QStringLiteral("%1：%2").arg(fileName, file.errorString()));
                continue;
            }
            contents = file.readAll();
            const QString decoded = QString::fromUtf8(contents.constData(), contents.size());
            if (contents.isEmpty() || decoded.toUtf8() != contents) {
                ++collected.failed;
                collected.warnings.append(
                    QStringLiteral("%1：不是有效的 UTF-8 文本").arg(fileName));
                continue;
            }
            book.title = QFileInfo(path).completeBaseName();
            book.characterCount = static_cast<int>(decoded.size());
            candidateText = decoded;
        }

        const QString titleKey = normalizedTitle(book.title);
        bool sameTitle = false;
        for (const KnownBook &entry : known) {
            if (normalizedTitleSimilarity(titleKey, entry.titleKey) < kTitleSimilarityThreshold) continue;
            sameTitle = true;
            break;
        }
        bool duplicate = false;
        if (sameTitle) {
            const ContentSignature signature = signatureForText(candidateText);
            if (signature.usable) {
                for (KnownBook &entry : known) {
                    if (normalizedTitleSimilarity(titleKey, entry.titleKey) < kTitleSimilarityThreshold) continue;
                    if (contentSimilarity(signature, knownSignature(entry)) < kContentSimilarityThreshold) continue;
                    duplicate = true;
                    break;
                }
            }
        }
        if (duplicate) {
            ++collected.skippedDuplicates;
            continue;
        }

        QString insertError;
        const QString id = insertBook(book, epub ? QStringLiteral(".epub") : QStringLiteral(".txt"),
                                      contents, cover, &insertError);
        if (id.isEmpty()) {
            ++collected.failed;
            collected.warnings.append(QStringLiteral("%1：%2").arg(fileName, insertError));
            continue;
        }
        ++collected.imported;
        known.append({m_books.first(), titleKey, {}, false});
    }

    int removed = 0;
    QString dedupeError;
    if (!mergeDuplicates(&removed, &dedupeError)) {
        if (error) *error = dedupeError;
        if (summary) *summary = collected;
        return false;
    }
    collected.removedDuplicates = removed;
    if (summary) *summary = collected;
    return true;
}

int LibraryStore::dedupe(QString *error)
{
    int removed = 0;
    if (!mergeDuplicates(&removed, error)) return -1;
    return removed;
}

bool LibraryStore::mergeDuplicates(int *removedCount, QString *error)
{
    const QList<Book> previousBooks = m_books;
    QList<Book> books = m_books;
    QList<QString> titleKeys;
    titleKeys.reserve(books.size());
    for (const Book &book : books) titleKeys.append(normalizedTitle(book.title));
    QList<ContentSignature> signatures(books.size());
    QList<bool> signatureReady(books.size(), false);
    const auto signatureAt = [this, &books, &signatures, &signatureReady](int index)
        -> const ContentSignature & {
        if (!signatureReady.at(index)) {
            signatures[index] = signatureForBook(textPath(books.at(index)));
            signatureReady[index] = true;
        }
        return signatures.at(index);
    };

    QList<bool> absorbed(books.size(), false);
    int removed = 0;
    for (int i = 0; i < books.size(); ++i) {
        if (absorbed.at(i)) continue;
        for (int j = i + 1; j < books.size(); ++j) {
            if (absorbed.at(j)) continue;
            if (normalizedTitleSimilarity(titleKeys.at(i), titleKeys.at(j)) < kTitleSimilarityThreshold) continue;
            const ContentSignature &first = signatureAt(i);
            const ContentSignature &second = signatureAt(j);
            if (!first.usable || !second.usable) continue;
            if (contentSimilarity(first, second) < kContentSimilarityThreshold) continue;
            if (keepsBetterReadingTrail(books.at(j), books.at(i))) {
                books.swapItemsAt(i, j);
                titleKeys.swapItemsAt(i, j);
                signatures.swapItemsAt(i, j);
                signatureReady.swapItemsAt(i, j);
            }
            absorbBook(books[i], books.at(j), signatures.at(i).digest == signatures.at(j).digest);
            absorbed[j] = true;
            ++removed;
        }
    }
    if (removed == 0) {
        if (removedCount) *removedCount = 0;
        return true;
    }

    QList<Book> kept;
    QList<Book> discarded;
    for (int i = 0; i < books.size(); ++i) {
        if (absorbed.at(i)) discarded.append(books.at(i));
        else kept.append(books.at(i));
    }
    m_books = kept;
    ensureCovers();
    if (!save(error)) {
        m_books = previousBooks;
        return false;
    }
    for (const Book &book : discarded) {
        QFile::remove(QDir(m_dataDirectory).filePath(book.storedFileName));
        removeCover(book);
    }
    if (removedCount) *removedCount = removed;
    return true;
}
