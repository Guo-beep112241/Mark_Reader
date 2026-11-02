#include "coverart.h"
#include "librarystore.h"
#include "readerwidget.h"
#include "retrowidgets.h"
#include "theme.h"

#include <QBuffer>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QIcon>
#include <QImage>
#include <QListWidget>
#include <QPainter>
#include <QTemporaryDir>
#include <QTest>
#include <QtTest>
#include <zip.h>

static bool writeZipEntry(zip_t *archive, const char *name, const QByteArray &contents)
{
    zip_source_t *source = zip_source_buffer(archive, contents.constData(), contents.size(), 0);
    return source && zip_file_add(archive, name, source, ZIP_FL_ENC_UTF_8) >= 0;
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

static bool writeTextFile(const QString &path, const QByteArray &contents)
{
    QDir().mkpath(QFileInfo(path).path());
    QFile file(path);
    return file.open(QIODevice::WriteOnly) && file.write(contents) == contents.size();
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
        QVERIFY(store.books().isEmpty());
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
