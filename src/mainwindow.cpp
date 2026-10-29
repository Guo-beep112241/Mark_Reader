#include "mainwindow.h"
#include "readerwidget.h"

#include <QApplication>
#include <QCloseEvent>
#include <QComboBox>
#include <QFile>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QSettings>
#include <QStandardPaths>
#include <QStackedWidget>
#include <QStatusBar>
#include <QTimer>
#include <QVBoxLayout>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent),
      m_store(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)),
      m_shelf(new QListWidget), m_pages(new QStackedWidget), m_reader(new ReaderWidget),
      m_saveTimer(new QTimer(this)), m_chapters(new QComboBox), m_bookmarks(new QComboBox),
      m_annotations(new QComboBox), m_searchField(new QLineEdit)
{
    setWindowTitle("Mark Reader");
    resize(1024, 740);
    QSettings settings;
    m_fontSize = settings.value("reader/fontSize", 18).toInt();
    m_darkTheme = settings.value("reader/darkTheme", false).toBool();

    auto *shelfPage = new QWidget;
    auto *shelfLayout = new QVBoxLayout(shelfPage);
    auto *shelfHeader = new QHBoxLayout;
    auto *title = new QLabel("我的书架");
    title->setStyleSheet("font-size: 26px; font-weight: 600;");
    auto *importButton = new QPushButton("导入书籍");
    auto *syncButton = new QPushButton("同步文件夹");
    auto *openButton = new QPushButton("打开选中");
    openButton->setEnabled(false);
    shelfHeader->addWidget(title);
    shelfHeader->addStretch();
    shelfHeader->addWidget(syncButton);
    shelfHeader->addWidget(openButton);
    shelfHeader->addWidget(importButton);
    shelfLayout->addLayout(shelfHeader);
    shelfLayout->addWidget(m_shelf);
    m_pages->addWidget(shelfPage);

    auto *readerPage = new QWidget;
    auto *readerLayout = new QVBoxLayout(readerPage);
    auto *toolbar = new QHBoxLayout;
    auto *backButton = new QPushButton("‹ 书架");
    auto *bookTitle = new QLabel;
    bookTitle->setObjectName("bookTitle");
    auto *smaller = new QPushButton("A−");
    auto *larger = new QPushButton("A+");
    auto *themeButton = new QPushButton;
    toolbar->addWidget(backButton);
    toolbar->addWidget(bookTitle, 1);
    toolbar->addWidget(new QLabel("章节"));
    toolbar->addWidget(m_chapters);
    toolbar->addWidget(smaller);
    toolbar->addWidget(larger);
    toolbar->addWidget(themeButton);
    readerLayout->addLayout(toolbar);

    auto *tools = new QHBoxLayout;
    auto *addBookmark = new QPushButton("添加书签");
    auto *addAnnotation = new QPushButton("划线/批注");
    m_bookmarks->setMinimumWidth(150);
    m_annotations->setMinimumWidth(260);
    tools->addWidget(addBookmark);
    tools->addWidget(m_bookmarks, 1);
    tools->addWidget(addAnnotation);
    tools->addWidget(m_annotations, 2);
    readerLayout->addLayout(tools);

    auto *searchTools = new QHBoxLayout;
    auto *searchButton = new QPushButton("查找下一处");
    m_searchField->setPlaceholderText("搜索本书全文…");
    searchTools->addWidget(m_searchField, 1);
    searchTools->addWidget(searchButton);
    readerLayout->addLayout(searchTools);
    readerLayout->addWidget(m_reader);
    m_pages->addWidget(readerPage);
    setCentralWidget(m_pages);

    m_reader->setPointSize(m_fontSize);
    applyTheme();
    themeButton->setText(m_darkTheme ? "亮色" : "深色");
    m_saveTimer->setSingleShot(true);
    m_saveTimer->setInterval(700);
    connect(m_saveTimer, &QTimer::timeout, this, &MainWindow::saveCurrentPosition);
    connect(m_reader, &ReaderWidget::positionChanged, m_saveTimer, qOverload<>(&QTimer::start));

    connect(importButton, &QPushButton::clicked, this, [this] {
        const QString path = QFileDialog::getOpenFileName(this, "导入书籍", {},
                                                          "书籍文件 (*.txt *.epub)");
        if (path.isEmpty()) return;
        QString error;
        const QString id = path.endsWith(".epub", Qt::CaseInsensitive)
            ? m_store.importEpubFile(path, &error) : m_store.importTextFile(path, &error);
        if (id.isEmpty()) QMessageBox::warning(this, "导入失败", error);
        refreshShelf();
    });
    connect(syncButton, &QPushButton::clicked, this, [this] {
        const QString defaultPath = QSettings().value("sync/folder").toString();
        const QString path = QFileDialog::getExistingDirectory(this, "选择云同步文件夹", defaultPath);
        if (path.isEmpty()) return;
        QString error;
        if (!m_store.syncFolder(path, &error)) QMessageBox::warning(this, "同步失败", error);
        else {
            QSettings().setValue("sync/folder", path);
            statusBar()->showMessage("书架已同步到所选文件夹。", 4000);
        }
        refreshShelf();
    });
    connect(m_shelf, &QListWidget::itemSelectionChanged, this, [this, openButton] {
        openButton->setEnabled(m_shelf->currentItem() != nullptr);
    });
    connect(openButton, &QPushButton::clicked, this, [this] { openBook(m_shelf->currentRow()); });
    connect(m_shelf, &QListWidget::itemActivated, this, [this](QListWidgetItem *item) {
        openBook(m_shelf->row(item));
    });
    connect(backButton, &QPushButton::clicked, this, [this] {
        saveCurrentPosition();
        m_pages->setCurrentIndex(0);
    });
    connect(smaller, &QPushButton::clicked, this, [this] {
        m_fontSize = qMax(12, m_fontSize - 1);
        m_reader->setPointSize(m_fontSize);
        QSettings().setValue("reader/fontSize", m_fontSize);
    });
    connect(larger, &QPushButton::clicked, this, [this] {
        m_fontSize = qMin(32, m_fontSize + 1);
        m_reader->setPointSize(m_fontSize);
        QSettings().setValue("reader/fontSize", m_fontSize);
    });
    connect(themeButton, &QPushButton::clicked, this, [this, themeButton] {
        m_darkTheme = !m_darkTheme;
        QSettings().setValue("reader/darkTheme", m_darkTheme);
        applyTheme();
        themeButton->setText(m_darkTheme ? "亮色" : "深色");
        if (!m_currentBookId.isEmpty()) {
            for (const Book &book : m_store.books())
                if (book.id == m_currentBookId) m_reader->showAnnotations(book.annotations, m_darkTheme);
        }
    });
    connect(m_chapters, qOverload<int>(&QComboBox::activated), this, [this](int index) {
        if (index >= 0) m_reader->setPosition(m_chapters->itemData(index).toInt());
    });
    connect(m_bookmarks, qOverload<int>(&QComboBox::activated), this, [this](int index) {
        if (index >= 0) m_reader->setPosition(m_bookmarks->itemData(index).toInt());
    });
    connect(m_annotations, qOverload<int>(&QComboBox::activated), this, [this](int index) {
        if (index >= 0) {
            const int start = m_annotations->itemData(index).toInt();
            const int end = m_annotations->itemData(index, Qt::UserRole + 1).toInt();
            m_reader->selectRange(start, end);
        }
    });
    connect(addBookmark, &QPushButton::clicked, this, [this] {
        if (m_currentBookId.isEmpty()) return;
        QString error;
        if (!m_store.addBookmark(m_currentBookId, m_reader->position(), &error)) {
            statusBar()->showMessage(error, 4000);
            return;
        }
        for (const Book &book : m_store.books()) if (book.id == m_currentBookId) reloadReaderMetadata(book);
    });
    connect(addAnnotation, &QPushButton::clicked, this, [this] {
        if (m_currentBookId.isEmpty() || m_reader->selectedText().isEmpty()) {
            statusBar()->showMessage("请先在正文中选中需要划线的文字。", 4000);
            return;
        }
        bool accepted = false;
        const QString note = QInputDialog::getText(this, "添加批注", "批注内容（可留空）：",
                                                   QLineEdit::Normal, {}, &accepted);
        if (!accepted) return;
        QString error;
        if (!m_store.addAnnotation(m_currentBookId, m_reader->selectionStart(), m_reader->selectionEnd(),
                                   m_reader->selectedText(), note, &error)) {
            QMessageBox::warning(this, "无法添加批注", error);
            return;
        }
        for (const Book &book : m_store.books()) if (book.id == m_currentBookId) reloadReaderMetadata(book);
    });
    connect(searchButton, &QPushButton::clicked, this, [this] {
        if (!m_reader->findText(m_searchField->text())) statusBar()->showMessage("没有找到匹配内容。", 3000);
    });
    connect(m_searchField, &QLineEdit::returnPressed, searchButton, &QPushButton::click);
    refreshShelf();
}

MainWindow::~MainWindow() { saveCurrentPosition(); }

void MainWindow::applyTheme()
{
    m_reader->setDarkTheme(m_darkTheme);
    setStyleSheet(m_darkTheme
        ? "QMainWindow, QWidget { background: #292c31; color: #e5e0d6; } QPushButton, QComboBox, QLineEdit { background: #393d44; color: #f0ece4; border: 1px solid #555b64; padding: 5px; } QListWidget { background: #22252a; color: #e5e0d6; }"
        : "QMainWindow, QWidget { background: #f7f5f0; color: #302c25; } QPushButton, QComboBox, QLineEdit { background: #ffffff; color: #302c25; border: 1px solid #d4d0c8; padding: 5px; } QListWidget { background: #ffffff; color: #302c25; }");
}

void MainWindow::refreshShelf()
{
    const QString selectedId = m_shelf->currentItem()
        ? m_shelf->currentItem()->data(Qt::UserRole).toString() : QString();
    m_shelf->clear();
    for (const Book &book : m_store.books()) {
        QString status = "尚未阅读";
        if (book.lastRead.isValid()) {
            const QString when = book.lastRead.toLocalTime().toString("MM-dd HH:mm");
            if (book.characterCount > 0) {
                const int percent = qBound(0, book.position * 100 / book.characterCount, 100);
                status = QString("%1% · 最近阅读 %2").arg(percent).arg(when);
            } else {
                status = QString("已读 %1 字 · 最近阅读 %2").arg(book.position).arg(when);
            }
        }
        auto *item = new QListWidgetItem(QString("%1  ·  %2").arg(book.title, status), m_shelf);
        item->setData(Qt::UserRole, book.id);
        if (book.id == selectedId) m_shelf->setCurrentItem(item);
    }
}

void MainWindow::reloadReaderMetadata(const Book &book)
{
    m_chapters->clear();
    for (int index = 0; index < book.chapterTitles.size(); ++index)
        m_chapters->addItem(book.chapterTitles.at(index), book.chapterOffsets.value(index));
    m_chapters->setVisible(!book.chapterTitles.isEmpty());
    m_bookmarks->clear();
    for (const Bookmark &bookmark : book.bookmarks)
        m_bookmarks->addItem(bookmark.label, bookmark.position);
    m_annotations->clear();
    for (const Annotation &annotation : book.annotations) {
        m_annotations->addItem(QString("%1 — %2").arg(annotation.quote.left(28), annotation.note), annotation.start);
        m_annotations->setItemData(m_annotations->count() - 1, annotation.end, Qt::UserRole + 1);
    }
    m_reader->showAnnotations(book.annotations, m_darkTheme);
}

void MainWindow::openBook(int row)
{
    const auto books = m_store.books();
    if (row < 0 || row >= books.size()) return;
    const Book book = books.at(row);
    QString text;
    if (book.storedFileName.endsWith(".epub", Qt::CaseInsensitive)) {
        EpubDocument document;
        QString error;
        if (!EpubDocument::load(m_store.textPath(book), &document, &error)) {
            QMessageBox::warning(this, "无法打开 EPUB", error);
            return;
        }
        text = document.text;
    } else {
        QFile file(m_store.textPath(book));
        if (!file.open(QIODevice::ReadOnly)) {
            QMessageBox::warning(this, "无法打开", "找不到书籍副本，请重新导入。");
            return;
        }
        text = QString::fromUtf8(file.readAll());
    }
    m_currentBookId = book.id;
    m_reader->setPlainText(text);
    findChild<QLabel *>("bookTitle")->setText(book.title);
    m_reader->setPosition(book.position);
    reloadReaderMetadata(book);
    m_pages->setCurrentIndex(1);
    QString error;
    m_store.savePosition(book.id, book.position, &error);
    refreshShelf();
}

void MainWindow::saveCurrentPosition()
{
    if (m_currentBookId.isEmpty()) return;
    QString error;
    if (!m_store.savePosition(m_currentBookId, m_reader->position(), &error))
        statusBar()->showMessage("阅读进度保存失败：" + error, 4000);
    refreshShelf();
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    saveCurrentPosition();
    event->accept();
}
