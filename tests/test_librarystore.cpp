#include "librarystore.h"

#include <QFile>
#include <QTemporaryDir>
#include <QTest>
#include <QtTest>

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

        LibraryStore reopened(dataPath);
        QCOMPARE(reopened.books().size(), 1);
        QCOMPARE(reopened.books().first().position, 6);
        QCOMPARE(reopened.books().first().characterCount, 12);
        QCOMPARE(reopened.books().first().id, id);
        QVERIFY(reopened.books().first().lastRead.isValid());
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
};

QTEST_GUILESS_MAIN(LibraryStoreTest)
#include "test_librarystore.moc"
