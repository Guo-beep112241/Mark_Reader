#include "annotationpanel.h"
#include "coverart.h"
#include "mainwindow.h"
#include "librarystore.h"
#include "readerwidget.h"
#include "retrowidgets.h"
#include "settingsdialog.h"
#include "theme.h"

#include <QBuffer>
#include <QComboBox>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFrame>
#include <QIcon>
#include <QImage>
#include <QListWidget>
#include <QMap>
#include <QPainter>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollBar>
#include <QSettings>
#include <QStandardPaths>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <QTextCursor>
#include <QTextDocument>
#include <QtTest>
#include <zip.h>

static bool writeZipEntry(zip_t *archive, const char *name, const QByteArray &contents)
{
    zip_source_t *source = zip_source_buffer(archive, contents.constData(), contents.size(), 0);
    return source && zip_file_add(archive, name, source, ZIP_FL_ENC_UTF_8) >= 0;
}

// 按条目名写入一个最小 EPUB；测试可以自由控制 container/OPF/XHTML 的路径和内容。
static bool writeEpub(const QString &path, const QMap<QString, QByteArray> &entries)
{
    int error = 0;
    zip_t *archive = zip_open(path.toUtf8().constData(), ZIP_CREATE | ZIP_TRUNCATE, &error);
    if (!archive) return false;
    for (auto entry = entries.cbegin(); entry != entries.cend(); ++entry) {
        if (writeZipEntry(archive, entry.key().toUtf8().constData(), entry.value())) continue;
        zip_discard(archive);
        return false;
    }
    return zip_close(archive) == 0;
}

static bool createEpub(const QString &path, const QByteArray &cover = {})
{
    int error = 0;
    zip_t *archive = zip_open(path.toUtf8().constData(), ZIP_CREATE | ZIP_TRUNCATE, &error);
    if (!archive) return false;
    const QByteArray container = "<container><rootfiles><rootfile full-path=\"OEBPS/book.opf\"/></rootfiles></container>";
    QByteArray package = "<package xmlns:dc=\"http://purl.org/dc/elements/1.1/\"><metadata>"
        "<dc:title>Sample EPUB</dc:title>";
    if (!cover.isEmpty()) package += "<meta name=\"cover\" content=\"cover-image\"/>";
    package += "</metadata><manifest><item id=\"chapter1\" href=\"chapter1.xhtml\"/>";
    if (!cover.isEmpty()) package += "<item id=\"cover-image\" href=\"images/cover.png\" media-type=\"image/png\"/>";
    package += "</manifest><spine><itemref idref=\"chapter1\"/></spine></package>";
    const QByteArray chapter = "<html xmlns=\"http://www.w3.org/1999/xhtml\"><body><h1>First Chapter</h1><p>Reading text.</p></body></html>";
    bool okay = writeZipEntry(archive, "META-INF/container.xml", container) &&
        writeZipEntry(archive, "OEBPS/book.opf", package) &&
        writeZipEntry(archive, "OEBPS/chapter1.xhtml", chapter);
    if (okay && !cover.isEmpty()) okay = writeZipEntry(archive, "OEBPS/images/cover.png", cover);
    if (!okay) {
        zip_discard(archive);
        return false;
    }
    return zip_close(archive) == 0;
}

static QByteArray pngBytes(const QImage &image)
{
    QByteArray bytes;
    QBuffer buffer(&bytes);
    buffer.open(QIODevice::WriteOnly);
    image.save(&buffer, "PNG");
    return bytes;
}

static QPushButton *buttonWithText(QWidget *root, const QString &text)
{
    for (QPushButton *button : root->findChildren<QPushButton *>())
        if (button->text() == text) return button;
    return nullptr;
}

static bool writeTextFile(const QString &path, const QByteArray &contents)
{
    QDir().mkpath(QFileInfo(path).path());
    QFile file(path);
    return file.open(QIODevice::WriteOnly) && file.write(contents) == contents.size();
}

static bool setFileModified(const QString &path, const QDateTime &time)
{
    QFile file(path);
    return file.open(QIODevice::ReadWrite)
        && file.setFileTime(time, QFileDevice::FileModificationTime);
}

// 生成多段带主题词的正文，让不同主题的书在分片特征上没有重合。
static QByteArray bookText(const QString &theme, int paragraphs)
{
    QString text;
    for (int index = 0; index < paragraphs; ++index) {
        text += QStringLiteral("%1第%2段：%1的故事从这里开始，人物、时间与地点都随段落变化。\n")
                    .arg(theme)
                    .arg(index);
    }
    return text.toUtf8();
}

class LibraryStoreTest : public QObject {
    Q_OBJECT
private slots:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
        QCoreApplication::setOrganizationName(QStringLiteral("MarkReaderTests"));
        QCoreApplication::setApplicationName(QStringLiteral("MarkReaderTests"));
    }

    void importsCopiesAndRestoresProgress()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString sourcePath = directory.filePath("sample.txt");
        QFile source(sourcePath);
        QVERIFY(source.open(QIODevice::WriteOnly));
        QCOMPARE(source.write("hello reader"), qint64(12));
        source.close();

        const QString dataPath = directory.filePath("app");
        LibraryStore store(dataPath);
        QString error;
        const QString id = store.importTextFile(sourcePath, &error);
        QVERIFY2(!id.isEmpty(), qPrintable(error));
        QCOMPARE(store.books().size(), 1);
        const Book imported = store.books().first();
        QVERIFY(QFile::exists(store.textPath(imported)));
        QVERIFY(QFile::exists(store.coverPath(imported)));
        QCOMPARE(QImage(store.coverPath(imported)).size(), QSize(600, 900));
        QCOMPARE(imported.title, QString("sample"));
        QCOMPARE(imported.characterCount, 12);
        QVERIFY(QFile::remove(sourcePath));
        QFile copied(store.textPath(imported));
        QVERIFY(copied.open(QIODevice::ReadOnly));
        QCOMPARE(copied.readAll(), QByteArray("hello reader"));
        QVERIFY(store.savePosition(id, 6, &error));
        QVERIFY(store.addBookmark(id, 6, &error));
        QVERIFY(store.addAnnotation(id, 0, 5, "hello", "opening word", &error));

        LibraryStore reopened(dataPath);
        QCOMPARE(reopened.books().size(), 1);
        QCOMPARE(reopened.books().first().position, 6);
        QCOMPARE(reopened.books().first().characterCount, 12);
        QCOMPARE(reopened.books().first().id, id);
        QVERIFY(reopened.books().first().lastRead.isValid());
        QCOMPARE(reopened.books().first().bookmarks.size(), 1);
        QCOMPARE(reopened.books().first().annotations.size(), 1);
        QCOMPARE(reopened.books().first().annotations.first().note, QString("opening word"));
        QVERIFY(reopened.books().first().annotations.first().createdAt.isValid());
    }

    void regeneratesMissingTextBookCoverOnLoad()
    {
        QTemporaryDir directory;
        const QString sourcePath = directory.filePath("missing-cover.txt");
        QFile source(sourcePath);
        QVERIFY(source.open(QIODevice::WriteOnly));
        source.write("A book whose cover file disappeared.");
        source.close();

        const QString dataPath = directory.filePath("app");
        LibraryStore store(dataPath);
        QString error;
        const QString id = store.importTextFile(sourcePath, &error);
        QVERIFY2(!id.isEmpty(), qPrintable(error));
        const Book book = store.books().first();
        const QString cover = store.coverPath(book);
        QVERIFY(QFile::remove(cover));

        LibraryStore reopened(dataPath);
        QVERIFY(QFile::exists(reopened.coverPath(reopened.books().first())));
        QCOMPARE(QImage(reopened.coverPath(reopened.books().first())).size(), QSize(600, 900));
    }

    void generatesDeterministicFallbackCovers()
    {
        const QImage first = CoverArt::renderFallback(QStringLiteral("城南旧事"));
        const QImage second = CoverArt::renderFallback(QStringLiteral("城南旧事"));
        QCOMPARE(first.size(), QSize(600, 900));
        QVERIFY(first == second);
        QVERIFY(first != CoverArt::renderFallback(QStringLiteral("人间词话")));

        QTemporaryDir directory;
        const QString path = directory.filePath("cover.png");
        QString error;
        QVERIFY2(CoverArt::save(first, path, &error), qPrintable(error));
        QImage loaded(path);
        QCOMPARE(loaded.size(), first.size());
        QVERIFY(!loaded.isNull());
    }

    void rejectsEmptyFiles()
    {
        QTemporaryDir directory;
        QFile source(directory.filePath("empty.txt"));
        QVERIFY(source.open(QIODevice::WriteOnly));
        source.close();
        LibraryStore store(directory.filePath("app"));
        QString error;
        QVERIFY(store.importTextFile(source.fileName(), &error).isEmpty());
        QVERIFY(!error.isEmpty());
        QVERIFY(store.books().isEmpty());
    }

    void rejectsInvalidUtf8()
    {
        QTemporaryDir directory;
        QFile source(directory.filePath("invalid.txt"));
        QVERIFY(source.open(QIODevice::WriteOnly));
        source.write(QByteArray::fromHex("fffe"));
        source.close();
        LibraryStore store(directory.filePath("app"));
        QString error;
        QVERIFY(store.importTextFile(source.fileName(), &error).isEmpty());
        QVERIFY(error.contains("UTF-8"));
        QVERIFY(error.contains("GBK"));
        QVERIFY(store.books().isEmpty());
    }

    void removesBookAndItsLocalFiles()
    {
        QTemporaryDir directory;
        const QString sourcePath = directory.filePath("remove-me.txt");
        QVERIFY(writeTextFile(sourcePath, "A book that will be removed."));
        LibraryStore store(directory.filePath("app"));
        QString error;
        const QString id = store.importTextFile(sourcePath, &error);
        QVERIFY2(!id.isEmpty(), qPrintable(error));
        const Book book = store.books().first();
        const QString textPath = store.textPath(book);
        const QString coverPath = store.coverPath(book);
        QVERIFY(QFile::exists(textPath));
        QVERIFY(QFile::exists(coverPath));

        QVERIFY2(store.removeBook(id, &error), qPrintable(error));
        QVERIFY(store.books().isEmpty());
        QVERIFY(!QFile::exists(textPath));
        QVERIFY(!QFile::exists(coverPath));
        // 移除只影响应用内副本，用户选择的原文件仍然保留。
        QVERIFY(QFile::exists(sourcePath));

        LibraryStore reopened(directory.filePath("app"));
        QVERIFY(reopened.books().isEmpty());
    }

    void removingOneBookKeepsTheOthers()
    {
        QTemporaryDir directory;
        const QString firstPath = directory.filePath("first.txt");
        const QString secondPath = directory.filePath("second.txt");
        QVERIFY(writeTextFile(firstPath, "first book"));
        QVERIFY(writeTextFile(secondPath, "second book"));
        LibraryStore store(directory.filePath("app"));
        QString error;
        const QString firstId = store.importTextFile(firstPath, &error);
        const QString secondId = store.importTextFile(secondPath, &error);
        QVERIFY(!firstId.isEmpty());
        QVERIFY(!secondId.isEmpty());

        QVERIFY(store.removeBook(firstId, &error));
        QCOMPARE(store.books().size(), 1);
        QCOMPARE(store.books().first().id, secondId);
        QVERIFY(QFile::exists(store.textPath(store.books().first())));

        QVERIFY(!store.removeBook(firstId, &error));
        QVERIFY(error.contains("找不到"));
        QCOMPARE(store.books().size(), 1);
    }

    void sortsBooksByMostRecentReading()
    {
        QTemporaryDir directory;
        const QString firstPath = directory.filePath("first.txt");
        const QString secondPath = directory.filePath("second.txt");
        for (const QString &path : {firstPath, secondPath}) {
            QFile file(path);
            QVERIFY(file.open(QIODevice::WriteOnly));
            file.write("book content");
        }

        LibraryStore store(directory.filePath("app"));
        QString error;
        const QString firstId = store.importTextFile(firstPath, &error);
        QVERIFY2(!firstId.isEmpty(), qPrintable(error));
        QTest::qWait(5);
        const QString secondId = store.importTextFile(secondPath, &error);
        QVERIFY2(!secondId.isEmpty(), qPrintable(error));
        QVERIFY(store.savePosition(firstId, 4, &error));

        const QList<Book> sorted = store.books();
        QCOMPARE(sorted.size(), 2);
        QCOMPARE(sorted.first().id, firstId);
        QCOMPARE(sorted.last().id, secondId);
    }

    void importsEpubAndExtractsSpineChapters()
    {
        QTemporaryDir directory;
        const QString sourcePath = directory.filePath("sample.epub");
        QVERIFY(createEpub(sourcePath));
        LibraryStore store(directory.filePath("app"));
        QString error;
        const QString id = store.importEpubFile(sourcePath, &error);
        QVERIFY2(!id.isEmpty(), qPrintable(error));
        const Book book = store.books().first();
        QCOMPARE(book.title, QString("Sample EPUB"));
        QCOMPARE(book.chapterTitles, QStringList({"First Chapter"}));
        QCOMPARE(book.chapterOffsets.size(), 1);
        QVERIFY(QFile::exists(store.textPath(book)));
        QVERIFY(QFile::exists(store.coverPath(book)));
        QCOMPARE(QImage(store.coverPath(book)).size(), QSize(600, 900));

        EpubDocument parsed;
        QVERIFY2(EpubDocument::load(store.textPath(book), &parsed, &error), qPrintable(error));
        QVERIFY(parsed.text.contains("Reading text."));
        QCOMPARE(parsed.chapters.size(), 1);
    }

    void importsEpubCoverAndStoresItAsPng()
    {
        QTemporaryDir directory;
        QImage artwork(300, 450, QImage::Format_ARGB32);
        artwork.fill(QColor("#6b2f2a"));
        const QString sourcePath = directory.filePath("covered.epub");
        QVERIFY(createEpub(sourcePath, pngBytes(artwork)));

        LibraryStore store(directory.filePath("app"));
        QString error;
        const QString id = store.importEpubFile(sourcePath, &error);
        QVERIFY2(!id.isEmpty(), qPrintable(error));
        const Book book = store.books().first();
        QVERIFY(QFile::exists(store.coverPath(book)));
        const QImage stored(store.coverPath(book));
        QCOMPARE(stored.size(), artwork.size());
        QCOMPARE(stored.pixelColor(10, 10), artwork.pixelColor(10, 10));

        EpubDocument parsed;
        QVERIFY2(EpubDocument::load(store.textPath(book), &parsed, &error), qPrintable(error));
        QVERIFY(!parsed.coverData.isEmpty());
    }

    void loadsEpubWithNestedOpfAndEncodedPaths()
    {
        QTemporaryDir directory;
        QImage artwork(180, 270, QImage::Format_ARGB32);
        artwork.fill(QColor("#2f4a3a"));
        const QByteArray container = R"(<?xml version="1.0" encoding="UTF-8"?>
<container xmlns="urn:oasis:names:tc:opendocument:xmlns:container" version="1.0">
  <rootfiles>
    <rootfile full-path="OPS/package/book%20one.opf" media-type="application/oebps-package+xml"/>
  </rootfiles>
</container>)";
        const QByteArray package = R"(<?xml version="1.0" encoding="UTF-8"?>
<package xmlns='http://www.idpf.org/2007/opf' version="3.0">
  <metadata xmlns:dc='http://purl.org/dc/elements/1.1/'>
    <dc:title>Nested &amp; Encoded</dc:title>
  </metadata>
  <manifest>
    <item id="ch1" href="text/chapter%201.xhtml" media-type="application/xhtml+xml"/>
    <item id="art" href="images/artwork.png" media-type="image/png" properties="cover-image"/>
  </manifest>
  <spine><itemref idref="ch1"/></spine>
</package>)";
        const QByteArray chapter = R"(<?xml version="1.0" encoding="UTF-8"?>
<html xmlns='http://www.w3.org/1999/xhtml'>
  <head><title>Head Chapter Title</title></head>
  <body><p>Nested path text.</p></body>
</html>)";
        const QString path = directory.filePath("nested.epub");
        QVERIFY(writeEpub(path, {
            {"META-INF/container.xml", container},
            {"OPS/package/book one.opf", package},
            {"OPS/package/text/chapter 1.xhtml", chapter},
            {"OPS/package/images/artwork.png", pngBytes(artwork)},
        }));

        EpubDocument parsed;
        QString error;
        QVERIFY2(EpubDocument::load(path, &parsed, &error), qPrintable(error));
        QCOMPARE(parsed.title, QString("Nested & Encoded"));
        QCOMPARE(parsed.chapters.size(), 1);
        QCOMPARE(parsed.chapters.first().title, QString("Head Chapter Title"));
        QCOMPARE(parsed.chapters.first().offset, 0);
        QVERIFY(parsed.text.contains("Nested path text."));
        QVERIFY(!parsed.coverData.isEmpty());
    }

    void prefersHeadingAndEpub2CoverMeta()
    {
        QTemporaryDir directory;
        QImage artwork(120, 180, QImage::Format_ARGB32);
        artwork.fill(QColor("#3b4f6b"));
        const QByteArray container =
            R"(<container><rootfiles><rootfile full-path="OPS/book.opf"/></rootfiles></container>)";
        const QByteArray package = R"(<package>
  <metadata><title>Cover Meta Book</title><meta name="cover" content="images/cover.png"/></metadata>
  <manifest>
    <item id="cover-art" href="images/cover.png" media-type="image/png"/>
    <item id="ch1" href="chapter1.xhtml" media-type="application/xhtml+xml"/>
  </manifest>
  <spine><itemref idref="ch1"/></spine>
</package>)";
        const QByteArray chapter = R"(<html><head><title>Ignored Head Title</title></head>
<body><h2>Heading Two</h2><p>Body text.</p></body></html>)";
        const QString path = directory.filePath("cover-meta.epub");
        QVERIFY(writeEpub(path, {
            {"META-INF/container.xml", container},
            {"OPS/book.opf", package},
            {"OPS/chapter1.xhtml", chapter},
            {"OPS/images/cover.png", pngBytes(artwork)},
        }));

        EpubDocument parsed;
        QString error;
        QVERIFY2(EpubDocument::load(path, &parsed, &error), qPrintable(error));
        QCOMPARE(parsed.title, QString("Cover Meta Book"));
        QCOMPARE(parsed.chapters.size(), 1);
        QCOMPARE(parsed.chapters.first().title, QString("Heading Two"));
        QVERIFY(parsed.text.contains("Heading Two"));
        QVERIFY(!parsed.coverData.isEmpty());
    }

    void skipsBrokenSpineEntriesAndKeepsOffsets()
    {
        QTemporaryDir directory;
        const QByteArray container =
            R"(<container><rootfiles><rootfile full-path="OPS/book.opf"/></rootfiles></container>)";
        const QByteArray package = R"(<package>
  <metadata><title>Broken Spine Book</title></metadata>
  <manifest>
    <item id="empty" href="empty.xhtml"/>
    <item id="broken" href="broken.xhtml"/>
    <item id="ch1" href="ch1.xhtml"/>
    <item id="ch2" href="ch2.xhtml"/>
  </manifest>
  <spine>
    <itemref idref="empty"/>
    <itemref idref="missing"/>
    <itemref idref="broken"/>
    <itemref idref="ch1"/>
    <itemref idref="ch2"/>
  </spine>
</package>)";
        const QString path = directory.filePath("broken-spine.epub");
        QVERIFY(writeEpub(path, {
            {"META-INF/container.xml", container},
            {"OPS/book.opf", package},
            {"OPS/empty.xhtml", "   "},
            {"OPS/broken.xhtml", "<html><body><p>unclosed"},
            {"OPS/ch1.xhtml", "<html><body><h1>One</h1><p>First body.</p></body></html>"},
            {"OPS/ch2.xhtml", "<html><body><h1>Two</h1><p>Second body.</p></body></html>"},
        }));

        EpubDocument parsed;
        QString error;
        QVERIFY2(EpubDocument::load(path, &parsed, &error), qPrintable(error));
        QCOMPARE(parsed.chapters.size(), 2);
        QCOMPARE(parsed.chapters.at(0).title, QString("One"));
        QCOMPARE(parsed.chapters.at(1).title, QString("Two"));
        QVERIFY(parsed.chapters.at(1).offset > parsed.chapters.at(0).offset);
        QVERIFY(parsed.text.mid(parsed.chapters.at(1).offset).startsWith("Two"));
    }

    void rejectsEpubWithoutReadableChapters()
    {
        QTemporaryDir directory;
        const QByteArray container =
            R"(<container><rootfiles><rootfile full-path="book.opf"/></rootfiles></container>)";
        const QByteArray package = R"(<package>
  <metadata><title>Empty Book</title></metadata>
  <manifest><item id="empty" href="empty.xhtml"/></manifest>
  <spine><itemref idref="empty"/></spine>
</package>)";
        const QString path = directory.filePath("empty.epub");
        QVERIFY(writeEpub(path, {
            {"META-INF/container.xml", container},
            {"book.opf", package},
            {"empty.xhtml", ""},
        }));

        EpubDocument parsed;
        QString error;
        QVERIFY(!EpubDocument::load(path, &parsed, &error));
        QVERIFY(error.contains("正文"));
    }

    void importsFolderRecursivelyAndSkipsDuplicates()
    {
        QTemporaryDir directory;
        const QString sync = directory.filePath("kindle");
        QVERIFY(writeTextFile(sync + "/城南旧事.txt", bookText("城南旧事", 30)));
        QVERIFY(writeTextFile(sync + "/子目录/人间词话.txt", bookText("人间词话", 30)));
        QVERIFY(writeTextFile(sync + "/notes.md", "not a book"));
        QVERIFY(writeTextFile(sync + "/empty.txt", QByteArray()));
        QVERIFY(writeTextFile(sync + "/invalid.txt", QByteArray::fromHex("fffe")));
        QVERIFY(writeTextFile(sync + "/books/00000000-0000-0000-0000-000000000001.txt",
                              bookText(QStringLiteral("旧版导出的副本"), 20)));
        QVERIFY(createEpub(sync + "/子目录/sample.epub"));

        LibraryStore store(directory.filePath("app"));
        FolderImportSummary summary;
        QString error;
        QVERIFY2(store.importFolder(sync, &summary, &error), qPrintable(error));
        QCOMPARE(summary.imported, 3);
        QCOMPARE(summary.skippedDuplicates, 0);
        QCOMPARE(summary.failed, 2);
        QCOMPARE(summary.removedDuplicates, 0);
        QCOMPARE(store.books().size(), 3);
        bool sawEpub = false;
        for (const Book &book : store.books()) {
            QVERIFY(QFile::exists(store.textPath(book)));
            QVERIFY(QFile::exists(store.coverPath(book)));
            if (!book.storedFileName.endsWith(".epub")) continue;
            sawEpub = true;
            QCOMPARE(book.title, QString("Sample EPUB"));
            QVERIFY(!book.chapterTitles.isEmpty());
        }
        QVERIFY(sawEpub);

        const QString trackedId = store.books().first().id;
        QVERIFY(store.savePosition(trackedId, 5, &error));
        FolderImportSummary again;
        QVERIFY2(store.importFolder(sync, &again, &error), qPrintable(error));
        QCOMPARE(again.imported, 0);
        QCOMPARE(again.skippedDuplicates, 3);
        QCOMPARE(again.failed, 2);
        QCOMPARE(store.books().size(), 3);
        QCOMPARE(store.books().first().id, trackedId);
        QCOMPARE(store.books().first().position, 5);
    }

    void folderImportMergesNearDuplicateCopies()
    {
        QTemporaryDir directory;
        const QString sync = directory.filePath("kindle");
        const QByteArray contents = bookText("城南旧事", 80);
        QVERIFY(writeTextFile(sync + "/城南旧事.txt", contents));
        QVERIFY(writeTextFile(sync + "/重复下载/城南旧事（副本）.txt",
                              contents + QStringLiteral("尾声：多年以后，我又回到那条胡同。").toUtf8()));
        // 同名但正文完全不同，或正文相同但书名完全不同，都不应被合并。
        QVERIFY(writeTextFile(sync + "/另一个版本/城南旧事.txt", bookText("三国演义", 80)));
        QVERIFY(writeTextFile(sync + "/人间词话.txt", contents));

        LibraryStore store(directory.filePath("app"));
        FolderImportSummary summary;
        QString error;
        QVERIFY2(store.importFolder(sync, &summary, &error), qPrintable(error));
        QCOMPARE(summary.imported, 3);
        QCOMPARE(summary.skippedDuplicates, 1);
        QCOMPARE(store.books().size(), 3);
        QCOMPARE(store.dedupe(&error), 0);
        QCOMPARE(store.books().size(), 3);
    }

    void dedupeKeepsOneCopyAndMergesReadingTrail()
    {
        QTemporaryDir directory;
        const QString source = directory.filePath("城南旧事.txt");
        const QByteArray contents = bookText("城南旧事", 60);
        QVERIFY(writeTextFile(source, contents));

        LibraryStore store(directory.filePath("app"));
        QString error;
        const QString firstId = store.importTextFile(source, &error);
        QVERIFY2(!firstId.isEmpty(), qPrintable(error));
        QVERIFY(store.savePosition(firstId, 12, &error));
        QVERIFY(store.addBookmark(firstId, 12, &error));
        QTest::qWait(5);
        const QString secondId = store.importTextFile(source, &error);
        QVERIFY2(!secondId.isEmpty(), qPrintable(error));
        QVERIFY(store.addBookmark(secondId, 20, &error));
        QCOMPARE(store.books().size(), 2);

        Book discarded;
        for (const Book &book : store.books()) {
            if (book.id == secondId) discarded = book;
        }
        QVERIFY(!discarded.storedFileName.isEmpty());

        QCOMPARE(store.dedupe(&error), 1);
        QCOMPARE(store.books().size(), 1);
        const Book kept = store.books().first();
        QCOMPARE(kept.id, firstId);
        QCOMPARE(kept.position, 12);
        QCOMPARE(kept.bookmarks.size(), 2);
        QVERIFY(!QFile::exists(store.textPath(discarded)));
        QVERIFY(!QFile::exists(store.coverPath(discarded)));

        LibraryStore reopened(directory.filePath("app"));
        QCOMPARE(reopened.books().size(), 1);
        QCOMPARE(reopened.books().first().bookmarks.size(), 2);
    }

    void folderImportAutomaticallyMergesExistingDuplicates()
    {
        QTemporaryDir directory;
        const QString source = directory.filePath("城南旧事.txt");
        QVERIFY(writeTextFile(source, bookText("城南旧事", 40)));
        LibraryStore store(directory.filePath("app"));
        QString error;
        QVERIFY(!store.importTextFile(source, &error).isEmpty());
        QVERIFY(!store.importTextFile(source, &error).isEmpty());
        QCOMPARE(store.books().size(), 2);

        const QString sync = directory.filePath("kindle");
        QVERIFY(QDir().mkpath(sync));
        FolderImportSummary summary;
        QVERIFY2(store.importFolder(sync, &summary, &error), qPrintable(error));
        QCOMPARE(summary.imported, 0);
        QCOMPARE(summary.removedDuplicates, 1);
        QCOMPARE(store.books().size(), 1);
    }

    void importFolderReportsMissingFolder()
    {
        QTemporaryDir directory;
        LibraryStore store(directory.filePath("app"));
        FolderImportSummary summary;
        QString error;
        QVERIFY(!store.importFolder(directory.filePath("missing"), &summary, &error));
        QVERIFY(error.contains(QStringLiteral("不存在")));
        QVERIFY(store.books().isEmpty());
    }

    void tracksSourceFileModificationTime()
    {
        QTemporaryDir directory;
        const QString source = directory.filePath("dated.txt");
        QVERIFY(writeTextFile(source, "text with a known modification time"));
        const QDateTime modified = QDateTime::fromSecsSinceEpoch(1700000000);
        QVERIFY(setFileModified(source, modified));

        LibraryStore store(directory.filePath("app"));
        QString error;
        QVERIFY(!store.importTextFile(source, &error).isEmpty());
        QCOMPARE(store.books().first().fileModifiedAt.toSecsSinceEpoch(),
                 modified.toSecsSinceEpoch());

        LibraryStore reopened(directory.filePath("app"));
        QCOMPARE(reopened.books().first().fileModifiedAt.toSecsSinceEpoch(),
                 modified.toSecsSinceEpoch());
    }

    void dedupeUsesNewerSourceFileToBreakReadingTie()
    {
        QTemporaryDir directory;
        const QString source = directory.filePath("城南旧事.txt");
        const QByteArray contents = bookText("城南旧事", 40);
        QVERIFY(writeTextFile(source, contents));
        QVERIFY(setFileModified(source, QDateTime::fromSecsSinceEpoch(1700000000)));

        LibraryStore store(directory.filePath("app"));
        QString error;
        const QString olderId = store.importTextFile(source, &error);
        QVERIFY2(!olderId.isEmpty(), qPrintable(error));
        QVERIFY(setFileModified(source, QDateTime::fromSecsSinceEpoch(1700000100)));
        const QString newerId = store.importTextFile(source, &error);
        QVERIFY2(!newerId.isEmpty(), qPrintable(error));
        QCOMPARE(store.books().size(), 2);

        QCOMPARE(store.dedupe(&error), 1);
        QCOMPARE(store.books().size(), 1);
        QCOMPARE(store.books().first().id, newerId);
    }

    void dedupeKeepsReadingTrailOverNewerFile()
    {
        QTemporaryDir directory;
        const QString source = directory.filePath("城南旧事.txt");
        QVERIFY(writeTextFile(source, bookText("城南旧事", 40)));
        QVERIFY(setFileModified(source, QDateTime::fromSecsSinceEpoch(1700000000)));

        LibraryStore store(directory.filePath("app"));
        QString error;
        const QString readId = store.importTextFile(source, &error);
        QVERIFY2(!readId.isEmpty(), qPrintable(error));
        QVERIFY(store.savePosition(readId, 12, &error));
        QVERIFY(store.addBookmark(readId, 12, &error));

        QVERIFY(setFileModified(source, QDateTime::fromSecsSinceEpoch(1700000100)));
        QVERIFY(!store.importTextFile(source, &error).isEmpty());
        QCOMPARE(store.books().size(), 2);

        QCOMPARE(store.dedupe(&error), 1);
        const Book kept = store.books().first();
        QCOMPARE(kept.id, readId);
        QCOMPARE(kept.position, 12);
        QCOMPARE(kept.bookmarks.size(), 1);
    }

    void folderImportKeepsChangedSourceAsSeparateBook()
    {
        QTemporaryDir directory;
        const QString sync = directory.filePath("kindle");
        const QString source = sync + "/城南旧事.txt";
        QByteArray firstContents;
        QByteArray secondContents;
        for (int index = 0; index < 120; ++index) {
            firstContents += QStringLiteral("第一版：胡同里的旧事第 %1 段。\n").arg(index).toUtf8();
            secondContents += QStringLiteral("第二版：战场上的故事第 %1 段。\n").arg(index).toUtf8();
        }
        QVERIFY(writeTextFile(source, firstContents));

        LibraryStore store(directory.filePath("app"));
        FolderImportSummary summary;
        QString error;
        QVERIFY2(store.importFolder(sync, &summary, &error), qPrintable(error));
        QCOMPARE(summary.imported, 1);
        const Book original = store.books().first();
        QVERIFY(store.savePosition(original.id, 10, &error));

        // 来源文件在另一端被改写：导入时不覆盖已有记录，而是作为新书并存。
        QVERIFY(writeTextFile(source, secondContents));
        FolderImportSummary again;
        QVERIFY2(store.importFolder(sync, &again, &error), qPrintable(error));
        QCOMPARE(again.imported, 1);
        QCOMPARE(store.books().size(), 2);
        bool keptOriginal = false;
        for (const Book &book : store.books()) {
            if (book.id != original.id) continue;
            keptOriginal = true;
            QCOMPARE(book.position, 10);
            QVERIFY(QFile::exists(store.textPath(book)));
        }
        QVERIFY(keptOriginal);
    }

    void defaultSyncFolderIsTheKindleICloudFolder()
    {
        const QString expected = QDir::home().filePath(
            QStringLiteral("Library/Mobile Documents/com~apple~CloudDocs/本机迁移/Documents/kindle"));
        QCOMPARE(LibraryStore::defaultSyncFolder(), expected);
        QVERIFY(LibraryStore::defaultSyncFolder().endsWith(QStringLiteral("/kindle")));
    }

    void readerSearchWrapsAndAnnotationsCanBeSelected()
    {
        ReaderWidget reader;
        reader.setPlainText("alpha beta alpha");
        QVERIFY(reader.findText("alpha"));
        QCOMPARE(reader.selectedText(), QString("alpha"));
        QVERIFY(reader.findText("alpha"));
        QVERIFY(reader.findText("alpha"));
        QCOMPARE(reader.selectionStart(), 0);
        reader.selectRange(6, 10);
        QCOMPARE(reader.selectedText(), QString("beta"));
        reader.setDarkTheme(true);
        QVERIFY(reader.darkTheme());
        QVERIFY(reader.styleSheet().contains(Theme::palette(true).ink.name()));
        reader.setDarkTheme(false);
        QVERIFY(!reader.darkTheme());
        QVERIFY(reader.styleSheet().contains(Theme::palette(false).ink.name()));
    }

    void themeProvidesCompleteRetroStyleSheet()
    {
        const Theme::Palette light = Theme::palette(false);
        const Theme::Palette dark = Theme::palette(true);
        QVERIFY(!Theme::serifFamilies().isEmpty());
        QVERIFY(!Theme::uiFont(14, true).families().isEmpty());

        const QString lightSheet = Theme::styleSheet(false);
        const QString darkSheet = Theme::styleSheet(true);
        QVERIFY(lightSheet.contains(light.accent.name()));
        QVERIFY(lightSheet.contains(light.gold.name()));
        QVERIFY(lightSheet.contains("QPushButton"));
        QVERIFY(lightSheet.contains("QComboBox::down-arrow"));
        QVERIFY(lightSheet.contains("noteSidebar"));
        QVERIFY(lightSheet.contains("selectionNoteButton"));
        QVERIFY(darkSheet.contains("noteCard"));
        QVERIFY(darkSheet.contains(dark.ink.name()));
        QVERIFY(darkSheet.contains(dark.backdropTop.name()));
        // 蓝色原生高亮不应再出现，样式表的占位符也必须全部替换。
        QVERIFY(!lightSheet.contains("#0078d7"));
        QVERIFY(!lightSheet.contains("@accent"));
        QVERIFY(!lightSheet.contains("@backdrop"));
        QVERIFY(!lightSheet.contains("@"));
    }

    void appIconResourceIsBundled()
    {
        const QIcon icon(QStringLiteral(":/theme/app-icon.png"));
        QVERIFY(!icon.isNull());
        QVERIFY(icon.availableSizes().contains(QSize(1024, 1024)));
        QVERIFY(!icon.pixmap(128, 128).isNull());
    }

    void fontChoicesComeFromInstalledFamilies()
    {
        const QStringList chinese = Theme::chineseFontChoices();
        const QStringList latin = Theme::latinFontChoices();
        QVERIFY(!chinese.isEmpty());
        QVERIFY(!latin.isEmpty());
        QVERIFY(!Theme::defaultChineseFamily().isEmpty());
        QVERIFY(!Theme::defaultLatinFamily().isEmpty());
        QVERIFY(chinese.contains(Theme::defaultChineseFamily()));
        QVERIFY(latin.contains(Theme::defaultLatinFamily()));
        QCOMPARE(Theme::readerFont(18).pointSize(), 18);
    }

    void readerUsesSeparateChineseAndLatinFonts()
    {
        ReaderWidget reader;
        reader.resize(520, 320);
        reader.setPlainText(QStringLiteral("汉字测试 abc 123 英文排版"));
        reader.setReaderFonts(QStringLiteral("Songti SC"), QStringLiteral("Times New Roman"), 18);
        QCOMPARE(reader.chineseFamily(), QStringLiteral("Songti SC"));
        QCOMPARE(reader.latinFamily(), QStringLiteral("Times New Roman"));

        const QString plain = reader.document()->toPlainText();
        QTextCursor cursor(reader.document());
        cursor.setPosition(plain.indexOf(QStringLiteral("abc")));
        const QStringList latinFamilies = cursor.charFormat().fontFamilies().toStringList();
        QVERIFY2(latinFamilies.contains(QStringLiteral("Times New Roman")),
                 qPrintable(latinFamilies.join(QLatin1Char(','))));
        cursor.setPosition(plain.indexOf(QStringLiteral("汉")));
        QVERIFY(cursor.charFormat().fontFamilies().toStringList().contains(QStringLiteral("Songti SC")));

        reader.show();
        QTest::qWait(30);
        const QImage songti = reader.grab().toImage();
        reader.setReaderFonts(QStringLiteral("PingFang SC"), QStringLiteral("Times New Roman"), 18);
        QTest::qWait(30);
        const QImage pingFang = reader.grab().toImage();
        QVERIFY(songti != pingFang);
        reader.setReaderFonts(QStringLiteral("PingFang SC"), QStringLiteral("Georgia"), 18);
        QTest::qWait(30);
        const QImage georgia = reader.grab().toImage();
        QVERIFY(pingFang != georgia);
    }

    void readerPageModeTurnsPagesWithKeyboardAndAnimation()
    {
        ReaderWidget reader;
        reader.resize(480, 300);
        QString text;
        for (int line = 0; line < 160; ++line)
            text += QStringLiteral("第%1行：翻页测试正文，用来撑出足够多的页面。\n").arg(line);
        reader.setPlainText(text);
        reader.show();
        QTest::qWait(50);
        reader.setPageMode(ReaderWidget::PageMode::Page);
        QTest::qWait(80);
        QCOMPARE(reader.verticalScrollBarPolicy(), Qt::ScrollBarAlwaysOff);
        QVERIFY(reader.pageCount() > 2);
        QCOMPARE(reader.currentPage(), 1);

        reader.setAnimationsEnabled(false);
        reader.nextPage();
        QTest::qWait(20);
        QCOMPARE(reader.currentPage(), 2);
        const int secondPagePosition = reader.position();
        reader.nextPage();
        QTest::qWait(20);
        QCOMPARE(reader.currentPage(), 3);
        reader.previousPage();
        QTest::qWait(20);
        QCOMPARE(reader.currentPage(), 2);
        QCOMPARE(reader.position(), secondPagePosition);

        reader.setAnimationsEnabled(true);
        reader.setFocus();
        QTest::keyClick(&reader, Qt::Key_Right);
        QTRY_COMPARE_WITH_TIMEOUT(reader.currentPage(), 3, 3000);
        QTest::keyClick(&reader, Qt::Key_Left);
        QTRY_COMPARE_WITH_TIMEOUT(reader.currentPage(), 2, 3000);

        reader.setAnimationsEnabled(false);
        for (int guard = 0; guard < 100 && reader.currentPage() < reader.pageCount(); ++guard) {
            reader.nextPage();
            QTest::qWait(5);
        }
        QCOMPARE(reader.currentPage(), reader.pageCount());

        reader.setPosition(0);
        reader.setAnimationsEnabled(true);
        reader.nextPage();
        QTest::qWait(30);
        reader.setPosition(0);
        QTest::qWait(450);
        QCOMPARE(reader.currentPage(), 1);

        reader.setPageMode(ReaderWidget::PageMode::Scroll);
        QCOMPARE(reader.verticalScrollBarPolicy(), Qt::ScrollBarAsNeeded);
    }

    void settingsDialogReturnsSelectedFontsAndSize()
    {
        SettingsDialog dialog;
        auto *chinese = dialog.findChild<QComboBox *>(QStringLiteral("chineseFontCombo"));
        auto *latin = dialog.findChild<QComboBox *>(QStringLiteral("latinFontCombo"));
        QVERIFY(chinese != nullptr);
        QVERIFY(latin != nullptr);
        QVERIFY(chinese->count() >= 2);
        QVERIFY(latin->count() >= 2);

        const QString chineseFamily = Theme::chineseFontChoices().first();
        const QString latinFamily = Theme::latinFontChoices().first();
        dialog.setChineseFont(chineseFamily);
        dialog.setLatinFont(latinFamily);
        dialog.setPointSize(24);
        QCOMPARE(dialog.chineseFont(), chineseFamily);
        QCOMPARE(dialog.latinFont(), latinFamily);
        QCOMPARE(dialog.pointSize(), 24);

        dialog.setChineseFont(QStringLiteral("不存在的字体"));
        QCOMPARE(dialog.chineseFont(), QString());
    }

    void annotationPanelShowsTimestampsAndSavesSelectedQuote()
    {
        AnnotationPanel panel;
        panel.resize(340, 720);
        panel.show();

        Annotation first;
        first.id = QStringLiteral("a");
        first.start = 4;
        first.end = 10;
        first.quote = QStringLiteral("长亭外，古道边");
        first.note = QStringLiteral("开篇的意境");
        first.createdAt = QDateTime(QDate(2026, 11, 3), QTime(9, 30));
        Annotation second;
        second.id = QStringLiteral("b");
        second.start = 20;
        second.end = 26;
        second.quote = QStringLiteral("晚风拂柳");
        second.createdAt = QDateTime(QDate(2026, 11, 2), QTime(21, 5));
        panel.setAnnotations({first, second});
        QCOMPARE(panel.noteCount(), 2);
        const QStringList entries = panel.displayedEntries();
        QVERIFY(entries.at(0).contains(QStringLiteral("2026-11-03 09:30")));
        QVERIFY(entries.at(0).contains(QStringLiteral("开篇的意境")));
        QVERIFY(entries.at(1).contains(QStringLiteral("2026-11-02 21:05")));

        const auto cards = panel.findChildren<QFrame *>(QStringLiteral("noteCard"));
        QCOMPARE(cards.size(), 2);
        QSignalSpy jump(&panel, &AnnotationPanel::annotationActivated);
        QTest::mouseClick(cards.first(), Qt::LeftButton, {}, cards.first()->rect().center());
        QCOMPARE(jump.count(), 1);
        QCOMPARE(jump.first().at(0).toInt(), 4);

        QSignalSpy save(&panel, &AnnotationPanel::saveRequested);
        panel.setSelection(4, 10, QStringLiteral("长亭外，古道边"));
        auto *editor = panel.findChild<QPlainTextEdit *>(QStringLiteral("noteEditor"));
        auto *saveButton = panel.findChild<QPushButton *>(QStringLiteral("primaryButton"));
        QVERIFY(editor != nullptr);
        QVERIFY(saveButton != nullptr);
        QVERIFY(editor->isEnabled());
        editor->setPlainText(QStringLiteral("随手记下"));
        QTest::mouseClick(saveButton, Qt::LeftButton);
        QCOMPARE(save.count(), 1);
        QCOMPARE(save.first().at(0).toInt(), 4);
        QCOMPARE(save.first().at(1).toInt(), 10);
        QCOMPARE(save.first().at(2).toString(), QStringLiteral("长亭外，古道边"));
        QCOMPARE(save.first().at(3).toString(), QStringLiteral("随手记下"));

        panel.clearComposer();
        QCOMPARE(panel.composerStart(), -1);
        QVERIFY(!editor->isEnabled());
        QVERIFY(!saveButton->isEnabled());
    }

    void mainWindowRestoresReaderPreferences()
    {
        QSettings settings;
        settings.clear();
        settings.setValue(QStringLiteral("reader/chineseFont"), QStringLiteral("PingFang SC"));
        settings.setValue(QStringLiteral("reader/latinFont"), QStringLiteral("Baskerville"));
        settings.setValue(QStringLiteral("reader/fontSize"), 20);
        settings.setValue(QStringLiteral("reader/pageMode"), QStringLiteral("page"));

        MainWindow window;
        auto *reader = window.findChild<ReaderWidget *>(QStringLiteral("reader"));
        QVERIFY(reader != nullptr);
        QCOMPARE(reader->chineseFamily(), QStringLiteral("PingFang SC"));
        QCOMPARE(reader->latinFamily(), QStringLiteral("Baskerville"));
        QCOMPARE(reader->pointSize(), 20);
        QCOMPARE(reader->pageMode(), ReaderWidget::PageMode::Page);
        settings.clear();
    }

    void mainWindowSavesSelectionAsTimestampedNote()
    {
        const QString dataDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
        QDir(dataDir).removeRecursively();
        QDir().mkpath(dataDir);
        QSettings().clear();

        QTemporaryDir sourceDir;
        QVERIFY(sourceDir.isValid());
        const QString sourcePath = sourceDir.filePath("integration.txt");
        const QByteArray contents = QStringLiteral(
            "第一段用来划词。\n\n第二段用来确认阅读位置和笔记可以一起保存。\n").toUtf8();
        QVERIFY(writeTextFile(sourcePath, contents));
        LibraryStore store(dataDir);
        QString error;
        QVERIFY2(!store.importTextFile(sourcePath, &error).isEmpty(), qPrintable(error));

        MainWindow window;
        window.show();
        QTest::qWait(200);
        auto *shelf = window.findChild<QListWidget *>(QStringLiteral("shelf"));
        QVERIFY(shelf != nullptr);
        QCOMPARE(shelf->count(), 1);
        shelf->setCurrentRow(0);
        QTest::qWait(50);
        QPushButton *open = buttonWithText(&window, QStringLiteral("打开选中"));
        QVERIFY(open != nullptr);
        open->click();
        QTest::qWait(250);

        auto *reader = window.findChild<ReaderWidget *>(QStringLiteral("reader"));
        QVERIFY(reader != nullptr);
        reader->selectRange(0, 5);
        QPushButton *annotate = buttonWithText(&window, QStringLiteral("划词笔记"));
        QVERIFY(annotate != nullptr);
        annotate->click();
        QTest::qWait(350);

        auto *panel = window.findChild<AnnotationPanel *>();
        QVERIFY(panel != nullptr);
        auto *editor = panel->findChild<QPlainTextEdit *>(QStringLiteral("noteEditor"));
        auto *save = panel->findChild<QPushButton *>(QStringLiteral("primaryButton"));
        QVERIFY(editor != nullptr);
        QVERIFY(save != nullptr);
        QVERIFY(editor->isEnabled());
        editor->setPlainText(QStringLiteral("集成测试笔记"));
        save->click();
        QTest::qWait(200);

        LibraryStore reopened(dataDir);
        QCOMPARE(reopened.books().size(), 1);
        const Book book = reopened.books().first();
        QCOMPARE(book.annotations.size(), 1);
        QCOMPARE(book.annotations.first().quote, QStringLiteral("第一段用来"));
        QCOMPARE(book.annotations.first().note, QStringLiteral("集成测试笔记"));
        QVERIFY(book.annotations.first().createdAt.isValid());
        QCOMPARE(panel->noteCount(), 1);
        QVERIFY(panel->displayedEntries().first().contains(QStringLiteral("记于 ")));
        QVERIFY(panel->displayedEntries().first().contains(QStringLiteral("集成测试笔记")));
    }

    void coverGridDelegatePaintsCards()
    {
        QListWidget shelf;
        shelf.resize(440, 360);
        shelf.setViewMode(QListView::IconMode);
        shelf.setGridSize(QSize(198, 320));
        auto *delegate = new CoverGridDelegate(&shelf);
        shelf.setItemDelegate(delegate);
        auto *item = new QListWidgetItem(&shelf);
        item->setText(QStringLiteral("城南旧事"));
        item->setData(CoverGridDelegate::StatusRole, QStringLiteral("42% · 11-02"));
        item->setData(CoverGridDelegate::ProgressRole, 42);
        item->setData(CoverGridDelegate::CoverRole,
                     QPixmap::fromImage(CoverArt::renderFallback(QStringLiteral("城南旧事"))));
        shelf.setCurrentItem(item);

        const QModelIndex index = shelf.model()->index(0, 0);
        QVERIFY(index.isValid());
        QImage canvas(198, 320, QImage::Format_ARGB32);
        canvas.fill(Qt::white);
        QStyleOptionViewItem option;
        option.rect = QRect(0, 0, 198, 320);
        option.state = QStyle::State_Enabled | QStyle::State_Selected;
        QPainter painter(&canvas);
        delegate->paint(&painter, option, index);
        painter.end();
        const QColor lightCard = canvas.pixelColor(20, 40);
        QVERIFY(canvas.pixelColor(99, 120) != QColor(Qt::white));
        delegate->setDarkTheme(true);
        canvas.fill(Qt::white);
        QPainter darkPainter(&canvas);
        delegate->paint(&darkPainter, option, index);
        darkPainter.end();
        QVERIFY(canvas.pixelColor(99, 120) != QColor(Qt::white));
        QVERIFY(canvas.pixelColor(20, 40) != lightCard);
    }
};

QTEST_MAIN(LibraryStoreTest)
#include "test_librarystore.moc"
