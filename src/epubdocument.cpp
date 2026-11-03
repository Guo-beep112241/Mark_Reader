#include "epubdocument.h"

#include <QDir>
#include <QFileInfo>
#include <QMap>
#include <QUrl>
#include <QXmlStreamReader>
#include <zip.h>

#include <memory>

namespace {
struct ManifestItem {
    QString href;
    QString mediaType;
    QString properties;
};

QByteArray readEntry(zip_t *archive, const QString &name, QString *error)
{
    const QByteArray encodedName = name.toUtf8();
    zip_stat_t stat;
    zip_stat_init(&stat);
    if (zip_stat(archive, encodedName.constData(), 0, &stat) != 0 || stat.size > 64 * 1024 * 1024) {
        if (error) *error = QStringLiteral("EPUB 条目不存在或过大：%1").arg(name);
        return {};
    }
    zip_file_t *entry = zip_fopen(archive, encodedName.constData(), 0);
    if (!entry) {
        if (error) *error = QStringLiteral("无法读取 EPUB 条目：%1").arg(name);
        return {};
    }
    QByteArray contents;
    contents.resize(static_cast<qsizetype>(stat.size));
    const zip_int64_t bytesRead = zip_fread(entry, contents.data(), stat.size);
    zip_fclose(entry);
    if (bytesRead < 0 || static_cast<zip_uint64_t>(bytesRead) != stat.size) {
        if (error) *error = QStringLiteral("EPUB 条目读取不完整：%1").arg(name);
        return {};
    }
    return contents;
}

// EPUB 内的 href 是相对包文件（OPF）的 URL 路径，可能带查询串、片段和百分号编码。
QString entryPath(const QString &basePath, const QString &href)
{
    QString path = href.section('#', 0, 0).section('?', 0, 0);
    path = QUrl::fromPercentEncoding(path.toUtf8());
    path.replace('\\', '/');
    // 畸形 EPUB 可能写出以 / 开头的“容器绝对路径”；zip 条目始终是相对路径。
    while (path.startsWith('/')) path.remove(0, 1);
    if (path.isEmpty()) return {};
    if (basePath.isEmpty() || basePath == QStringLiteral(".")) return QDir::cleanPath(path);
    return QDir::cleanPath(basePath + '/' + path);
}

QString extractChapterText(const QByteArray &contents, QString *title, QString *error)
{
    QXmlStreamReader xml(contents);
    QString result;
    QString headTitle;
    bool inBody = false;
    while (!xml.atEnd()) {
        xml.readNext();
        if (xml.isStartElement()) {
            const QString name = xml.name().toString().toLower();
            if (name == "body") {
                inBody = true;
            } else if (!inBody) {
                // 没有标题元素时，XHTML 的 <title> 比文件名更适合作为章节名。
                if (name == "title" && headTitle.isEmpty())
                    headTitle = xml.readElementText(QXmlStreamReader::IncludeChildElements).trimmed();
            } else if (name == "h1" || name == "h2" || name == "h3" || name == "h4"
                       || name == "h5" || name == "h6") {
                const QString heading =
                    xml.readElementText(QXmlStreamReader::IncludeChildElements).trimmed();
                if (title->isEmpty() && !heading.isEmpty()) *title = heading;
                if (!heading.isEmpty()) result += heading + "\n\n";
            } else if (name == "p" || name == "div" || name == "br" || name == "li") {
                if (!result.isEmpty() && !result.endsWith('\n')) result += '\n';
            }
        } else if (xml.isCharacters() && inBody) {
            result += xml.text().toString();
        } else if (xml.isEndElement()) {
            const QString name = xml.name().toString().toLower();
            if (name == "body") inBody = false;
            if (name == "p" || name == "div" || name == "li") {
                if (!result.endsWith('\n')) result += '\n';
            }
        }
    }
    if (xml.hasError()) {
        if (error) *error = QStringLiteral("EPUB 章节 XHTML 格式错误：%1").arg(xml.errorString());
        return {};
    }
    if (title->isEmpty()) *title = headTitle;
    return result.trimmed();
}
}

bool EpubDocument::load(const QString &path, EpubDocument *document, QString *error)
{
    int zipError = 0;
    std::unique_ptr<zip_t, decltype(&zip_close)> archive(zip_open(path.toUtf8().constData(), ZIP_RDONLY, &zipError), &zip_close);
    if (!archive) {
        if (error) *error = QStringLiteral("无法打开 EPUB 文件。");
        return false;
    }
    const QByteArray container = readEntry(archive.get(), "META-INF/container.xml", error);
    if (container.isEmpty()) return false;
    QXmlStreamReader containerXml(container);
    QString packagePath;
    while (!containerXml.atEnd()) {
        containerXml.readNext();
        if (containerXml.isStartElement() && containerXml.name() == "rootfile") {
            packagePath = containerXml.attributes().value("full-path").toString().trimmed();
            break;
        }
    }
    if (packagePath.isEmpty()) {
        if (error) *error = QStringLiteral("EPUB 缺少 package.opf 索引。");
        return false;
    }
    packagePath = QUrl::fromPercentEncoding(packagePath.toUtf8());
    const QByteArray packageData = readEntry(archive.get(), packagePath, error);
    if (packageData.isEmpty()) return false;
    QXmlStreamReader packageXml(packageData);
    QMap<QString, ManifestItem> manifest;
    QStringList spine;
    QString bookTitle;
    QString coverId;
    while (!packageXml.atEnd()) {
        packageXml.readNext();
        if (!packageXml.isStartElement()) continue;
        const QString element = packageXml.name().toString();
        if (element == "title" && bookTitle.isEmpty()) {
            bookTitle = packageXml.readElementText(QXmlStreamReader::IncludeChildElements).trimmed();
        } else if (element == "item") {
            const auto attributes = packageXml.attributes();
            manifest.insert(attributes.value("id").toString(),
                            {attributes.value("href").toString(),
                             attributes.value("media-type").toString(),
                             attributes.value("properties").toString()});
        } else if (element == "itemref") {
            spine.append(packageXml.attributes().value("idref").toString());
        } else if (element == "meta") {
            const auto attributes = packageXml.attributes();
            if (attributes.value("name").toString().compare("cover", Qt::CaseInsensitive) == 0)
                coverId = attributes.value("content").toString();
        }
    }
    if (packageXml.hasError() || spine.isEmpty()) {
        if (error) *error = QStringLiteral("EPUB package 索引无效或没有阅读章节。");
        return false;
    }
    const QString basePath = QFileInfo(packagePath).path();
    EpubDocument parsed;
    parsed.title = bookTitle;

    QString coverHref;
    if (!coverId.isEmpty() && manifest.contains(coverId)) coverHref = manifest.value(coverId).href;
    if (coverHref.isEmpty() && !coverId.isEmpty()) {
        // 少数 EPUB 把 cover 元数据直接写成图片的相对路径，而不是 manifest id。
        for (auto item = manifest.cbegin(); item != manifest.cend(); ++item) {
            if (entryPath(basePath, item->href) == entryPath(basePath, coverId)) {
                coverHref = item->href;
                break;
            }
        }
    }
    if (coverHref.isEmpty()) {
        for (auto item = manifest.cbegin(); item != manifest.cend(); ++item) {
            if (item->properties.split(' ', Qt::SkipEmptyParts).contains("cover-image")) {
                coverHref = item->href;
                break;
            }
        }
    }
    if (coverHref.isEmpty()) {
        for (auto item = manifest.cbegin(); item != manifest.cend(); ++item) {
            const QString href = item->href.toLower();
            if (item->mediaType.startsWith("image/") && href.contains("cover")) {
                coverHref = item->href;
                break;
            }
        }
    }
    if (!coverHref.isEmpty()) {
        parsed.coverData =
            readEntry(archive.get(), entryPath(basePath, coverHref), nullptr);
    }

    for (const QString &id : spine) {
        if (!manifest.contains(id)) continue;
        const QString href = manifest.value(id).href;
        const QString chapterPath = entryPath(basePath, href);
        if (chapterPath.isEmpty()) continue;
        QString chapterError;
        const QByteArray chapterData = readEntry(archive.get(), chapterPath, &chapterError);
        // 空章节或单个损坏的章节跳过，不因为一个条目丢失整本书的正文。
        if (chapterData.isEmpty()) continue;
        EpubChapter chapter;
        chapter.text = extractChapterText(chapterData, &chapter.title, &chapterError);
        if (chapter.title.isEmpty()) chapter.title = QFileInfo(chapterPath).completeBaseName();
        if (chapter.text.isEmpty()) continue;
        if (!parsed.text.isEmpty()) parsed.text += "\n\n";
        chapter.offset = parsed.text.size();
        parsed.text += chapter.title + "\n\n" + chapter.text;
        parsed.chapters.append(chapter);
    }
    if (parsed.chapters.isEmpty()) {
        if (error) *error = QStringLiteral("EPUB 中没有可显示的正文。");
        return false;
    }
    if (parsed.title.isEmpty()) parsed.title = QFileInfo(path).completeBaseName();
    *document = parsed;
    return true;
}
