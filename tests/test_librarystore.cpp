#include "librarystore.h"
#include "readerwidget.h"

#include <QFile>
#include <QDir>
#include <QTemporaryDir>
#include <QTest>
#include <QtTest>
#include <zip.h>

static bool writeZipEntry(zip_t *archive, const char *name, const QByteArray &contents)
{
    zip_source_t *source = zip_source_buffer(archive, contents.constData(), contents.size(), 0);
    return source && zip_file_add(archive, name, source, ZIP_FL_ENC_UTF_8) >= 0;
}

static bool createEpub(const QString &path)
{
    int error = 0;
    zip_t *archive = zip_open(path.toUtf8().constData(), ZIP_CREATE | ZIP_TRUNCATE, &error);
    if (!archive) return false;
    const QByteArray container = "<container><rootfiles><rootfile full-path=\"OEBPS/book.opf\"/></rootfiles></container>";
    const QByteArray package = "<package xmlns:dc=\"http://purl.org/dc/elements/1.1/\"><metadata><dc:title>Sample EPUB</dc:title></metadata>"
        "<manifest><item id=\"chapter1\" href=\"chapter1.xhtml\"/></manifest>"
        "<spine><itemref idref=\"chapter1\"/></spine></package>";
    const QByteArray chapter = "<html xmlns=\"http://www.w3.org/1999/xhtml\"><body><h1>First Chapter</h1><p>Reading text.</p></body></html>";
    const bool okay = writeZipEntry(archive, "META-INF/container.xml", container) &&
        writeZipEntry(archive, "OEBPS/book.opf", package) &&
        writeZipEntry(archive, "OEBPS/chapter1.xhtml", chapter);
    if (!okay) {
        zip_discard(archive);
        return false;
    }
    return zip_close(archive) == 0;
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

        EpubDocument parsed;
        QVERIFY2(EpubDocument::load(store.textPath(book), &parsed, &error), qPrintable(error));
        QVERIFY(parsed.text.contains("Reading text."));
        QCOMPARE(parsed.chapters.size(), 1);
    }

    void syncFolderMergesBooksAndReadingProgress()
    {
        QTemporaryDir directory;
        const QString sourcePath = directory.filePath("cloud.txt");
        QFile source(sourcePath);
        QVERIFY(source.open(QIODevice::WriteOnly));
        source.write("A book for syncing across devices.");
        source.close();
        LibraryStore first(directory.filePath("device-one"));
        LibraryStore second(directory.filePath("device-two"));
        QString error;
        const QString id = first.importTextFile(sourcePath, &error);
        QVERIFY2(!id.isEmpty(), qPrintable(error));
        const QString cloud = directory.filePath("cloud-folder");
        QVERIFY2(first.syncFolder(cloud, &error), qPrintable(error));
        QVERIFY2(second.syncFolder(cloud, &error), qPrintable(error));
        QCOMPARE(second.books().size(), 1);
        QVERIFY(QFile::exists(second.textPath(second.books().first())));
        QVERIFY(second.savePosition(id, 10, &error));
        QVERIFY2(second.syncFolder(cloud, &error), qPrintable(error));
        QVERIFY2(first.syncFolder(cloud, &error), qPrintable(error));
        QCOMPARE(first.books().first().position, 10);
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
        QVERIFY(reader.styleSheet().contains("#22252a"));
        reader.setDarkTheme(false);
        QVERIFY(reader.styleSheet().contains("#fbf8f0"));
    }
};

QTEST_MAIN(LibraryStoreTest)
#include "test_librarystore.moc"
