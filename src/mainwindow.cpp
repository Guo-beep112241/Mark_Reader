#include "mainwindow.h"

#include "readerwidget.h"
#include "retrowidgets.h"
#include "theme.h"

#include <QApplication>
#include <QCloseEvent>
#include <QComboBox>
#include <QFile>
#include <QFileDialog>
#include <QFrame>
#include <QGraphicsDropShadowEffect>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
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
      m_annotations(new QComboBox), m_searchField(new QLineEdit), m_bookTitle(new QLabel),
      m_chapterLabel(new QLabel), m_emptyState(new QLabel), m_shelfCount(new QLabel), m_readerFooter(new QLabel),
      m_themeButton(new QPushButton), m_shelfPage(new PaperWidget), m_readerPage(new PaperWidget),
      m_shelfRule(new OrnamentRule), m_readerRule(new OrnamentRule), m_shelfDelegate(nullptr)
{
    setWindowTitle("Mark Reader");
    resize(1100, 780);
    setMinimumSize(900, 640);
    setFont(Theme::uiFont(14));
    QSettings settings;
    m_fontSize = settings.value("reader/fontSize", 18).toInt();
    m_darkTheme = settings.value("reader/darkTheme", false).toBool();

    // ---- 书架页 -------------------------------------------------------
    m_shelfPage->setObjectName("page");
    m_shelfPage->setDarkTheme(m_darkTheme);
    auto *shelfLayout = new QVBoxLayout(m_shelfPage);
    shelfLayout->setContentsMargins(38, 26, 38, 22);
    shelfLayout->setSpacing(0);

    auto *headerRow = new QHBoxLayout;
    headerRow->setSpacing(10);
    auto *titleColumn = new QVBoxLayout;
    titleColumn->setSpacing(2);
    auto *title = new QLabel("我的书架");
    title->setObjectName("shelfTitle");
    auto *subtitle = new QLabel("MARK READER · 私人藏书");
    subtitle->setObjectName("shelfSubtitle");
    titleColumn->addWidget(title);
    titleColumn->addWidget(subtitle);
    headerRow->addLayout(titleColumn);
    headerRow->addStretch(1);
    auto *syncButton = new QPushButton("同步文件夹");
    auto *openButton = new QPushButton("打开选中");
    auto *importButton = new QPushButton("导入书籍");
    importButton->setObjectName("primaryButton");
    openButton->setEnabled(false);
    headerRow->addWidget(syncButton, 0, Qt::AlignBottom);
    headerRow->addWidget(openButton, 0, Qt::AlignBottom);
    headerRow->addWidget(importButton, 0, Qt::AlignBottom);
    shelfLayout->addLayout(headerRow);
    shelfLayout->addSpacing(10);
    m_shelfRule->setDarkTheme(m_darkTheme);
    shelfLayout->addWidget(m_shelfRule);
    shelfLayout->addSpacing(2);

    auto *countRow = new QHBoxLayout;
    m_shelfCount->setObjectName("shelfCount");
    countRow->addStretch(1);
    countRow->addWidget(m_shelfCount);
    shelfLayout->addLayout(countRow);

    m_shelf->setObjectName("shelf");
    m_shelf->setViewMode(QListView::IconMode);
    m_shelf->setResizeMode(QListView::Adjust);
    m_shelf->setMovement(QListView::Static);
    m_shelf->setUniformItemSizes(true);
    m_shelf->setSelectionMode(QAbstractItemView::SingleSelection);
    m_shelf->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_shelf->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_shelf->setFrameShape(QFrame::NoFrame);
    m_shelf->setGridSize(QSize(198, 320));
    m_shelf->setIconSize(QSize(142, 213));
    m_shelf->setSpacing(0);
    m_shelf->setMouseTracking(true);
    m_shelfDelegate = new CoverGridDelegate(m_shelf);
    m_shelfDelegate->setDarkTheme(m_darkTheme);
    m_shelf->setItemDelegate(m_shelfDelegate);

    m_emptyState->setText("书架还空着\n\n导入一本 TXT 或 EPUB，\n封面会自动生成。");
    m_emptyState->setObjectName("emptyState");
    m_emptyState->setAlignment(Qt::AlignCenter);
    m_emptyState->setWordWrap(true);
    shelfLayout->addWidget(m_emptyState, 1);
    shelfLayout->addWidget(m_shelf, 1);
    m_pages->addWidget(m_shelfPage);

    // ---- 阅读页 -------------------------------------------------------
    m_readerPage->setObjectName("page");
    m_readerPage->setDarkTheme(m_darkTheme);
    auto *readerLayout = new QVBoxLayout(m_readerPage);
    readerLayout->setContentsMargins(34, 20, 34, 18);
    readerLayout->setSpacing(0);

    auto *toolbar = new QHBoxLayout;
    toolbar->setSpacing(8);
    auto *backButton = new QPushButton("‹ 书架");
    auto *bookTitleColumn = new QVBoxLayout;
    bookTitleColumn->setSpacing(0);
    m_bookTitle->setObjectName("bookTitle");
    m_bookTitle->setTextFormat(Qt::PlainText);
    m_bookTitle->setText("选择一本书");
    auto *bookSubtitle = new QLabel("MARK READER · 阅读室");
    bookSubtitle->setObjectName("bookSubtitle");
    bookTitleColumn->addWidget(m_bookTitle);
    bookTitleColumn->addWidget(bookSubtitle);
    toolbar->addWidget(backButton);
    toolbar->addSpacing(6);
    toolbar->addLayout(bookTitleColumn, 1);
    m_chapterLabel = new QLabel("章节");
    m_chapterLabel->setObjectName("toolLabel");
    m_chapters->setMinimumWidth(150);
    toolbar->addWidget(m_chapterLabel);
    toolbar->addWidget(m_chapters);
    auto *smaller = new QPushButton("A−");
    smaller->setToolTip("缩小字号");
    auto *larger = new QPushButton("A+");
    larger->setToolTip("放大字号");
    m_themeButton->setToolTip("切换夜读配色");
    toolbar->addWidget(smaller);
    toolbar->addWidget(larger);
    toolbar->addWidget(m_themeButton);
    readerLayout->addLayout(toolbar);
    readerLayout->addSpacing(10);
    m_readerRule->setDarkTheme(m_darkTheme);
    readerLayout->addWidget(m_readerRule);
    readerLayout->addSpacing(8);

    auto *toolsRow = new QHBoxLayout;
    toolsRow->setSpacing(8);
    auto *addBookmark = new QPushButton("添加书签");
    auto *addAnnotation = new QPushButton("划线 / 批注");
    auto *bookmarkLabel = new QLabel("书签");
    bookmarkLabel->setObjectName("toolLabel");
    auto *annotationLabel = new QLabel("批注");
    annotationLabel->setObjectName("toolLabel");
    m_bookmarks->setMinimumWidth(150);
    m_annotations->setMinimumWidth(240);
    toolsRow->addWidget(addBookmark);
    toolsRow->addWidget(bookmarkLabel);
    toolsRow->addWidget(m_bookmarks, 1);
    toolsRow->addWidget(addAnnotation);
    toolsRow->addWidget(annotationLabel);
    toolsRow->addWidget(m_annotations, 2);
    readerLayout->addLayout(toolsRow);
    readerLayout->addSpacing(8);

    auto *searchRow = new QHBoxLayout;
    searchRow->setSpacing(8);
    auto *searchLabel = new QLabel("检索");
    searchLabel->setObjectName("toolLabel");
    m_searchField->setPlaceholderText("在本书中检索…");
    auto *searchButton = new QPushButton("查找下一处");
    searchRow->addWidget(searchLabel);
    searchRow->addWidget(m_searchField, 1);
    searchRow->addWidget(searchButton);
    readerLayout->addLayout(searchRow);
    readerLayout->addSpacing(12);

    auto *readerFrame = new QFrame;
    readerFrame->setObjectName("readerFrame");
    auto *frameLayout = new QVBoxLayout(readerFrame);
    frameLayout->setContentsMargins(8, 8, 8, 8);
    frameLayout->addWidget(m_reader);
    auto *shadow = new QGraphicsDropShadowEffect(readerFrame);
    shadow->setBlurRadius(28);
    shadow->setOffset(0, 8);
    shadow->setColor(QColor(40, 26, 12, 95));
    readerFrame->setGraphicsEffect(shadow);
    readerLayout->addWidget(readerFrame, 1);
    readerLayout->addSpacing(8);

    auto *footerRow = new QHBoxLayout;
    m_readerFooter->setObjectName("readerFooter");
    auto *readerHint = new QLabel("滚轮阅读 · 自动保存进度");
    readerHint->setObjectName("readerFooter");
    footerRow->addWidget(m_readerFooter);
    footerRow->addStretch(1);
    footerRow->addWidget(readerHint);
    readerLayout->addLayout(footerRow);
    m_pages->addWidget(m_readerPage);
    setCentralWidget(m_pages);

    m_reader->setPointSize(m_fontSize);
    applyTheme();
    updateReaderFooter();

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
        if (!m_store.syncFolder(path, &error)) {
            QMessageBox::warning(this, "同步失败", error);
        } else {
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
        updateReaderFooter();
    });
    connect(larger, &QPushButton::clicked, this, [this] {
        m_fontSize = qMin(32, m_fontSize + 1);
        m_reader->setPointSize(m_fontSize);
        QSettings().setValue("reader/fontSize", m_fontSize);
        updateReaderFooter();
    });
    connect(m_themeButton, &QPushButton::clicked, this, [this] {
        m_darkTheme = !m_darkTheme;
        QSettings().setValue("reader/darkTheme", m_darkTheme);
        applyTheme();
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
    setStyleSheet(Theme::styleSheet(m_darkTheme));
    m_shelfPage->setDarkTheme(m_darkTheme);
    m_readerPage->setDarkTheme(m_darkTheme);
    m_shelfRule->setDarkTheme(m_darkTheme);
    m_readerRule->setDarkTheme(m_darkTheme);
    m_shelfDelegate->setDarkTheme(m_darkTheme);
    m_reader->setDarkTheme(m_darkTheme);
    m_themeButton->setText(m_darkTheme ? "晨读" : "夜读");
    m_themeButton->setToolTip(m_darkTheme ? "切换到纸色日间阅读" : "切换到夜读配色");
    m_shelf->viewport()->update();
    if (!m_currentBookId.isEmpty()) {
        for (const Book &book : m_store.books())
            if (book.id == m_currentBookId) m_reader->showAnnotations(book.annotations, m_darkTheme);
    }
}

QPixmap MainWindow::coverFor(const Book &book)
{
    const QPixmap cached = m_coverCache.value(book.id);
    if (!cached.isNull()) return cached;
    QPixmap cover;
    const QString path = m_store.coverPath(book);
    if (!path.isEmpty() && cover.load(path)) m_coverCache.insert(book.id, cover);
    return cover;
}

void MainWindow::refreshShelf()
{
    const QString selectedId = m_shelf->currentItem()
        ? m_shelf->currentItem()->data(Qt::UserRole).toString() : QString();
    m_shelf->clear();
    const QList<Book> books = m_store.books();
    for (const Book &book : books) {
        int percent = -1;
        QString compactStatus = "尚未阅读";
        QString detail = "尚未阅读";
        if (book.lastRead.isValid()) {
            const QString when = book.lastRead.toLocalTime().toString("yyyy-MM-dd HH:mm");
            if (book.characterCount > 0) {
                percent = qBound(0, book.position * 100 / book.characterCount, 100);
                compactStatus = QString("%1% · %2").arg(percent).arg(book.lastRead.toLocalTime().toString("MM-dd"));
                detail = QString("最近阅读 %1 · 已读 %2%").arg(when).arg(percent);
            } else {
                compactStatus = QString("已读 %1 字").arg(book.position);
                detail = QString("最近阅读 %1 · 已读 %2 字").arg(when).arg(book.position);
            }
        }
        auto *item = new QListWidgetItem(m_shelf);
        item->setText(book.title);
        item->setData(Qt::UserRole, book.id);
        item->setData(CoverGridDelegate::StatusRole, compactStatus);
        item->setData(CoverGridDelegate::ProgressRole, percent);
        item->setData(CoverGridDelegate::CoverRole, coverFor(book));
        item->setToolTip(QString("%1\n%2").arg(book.title, detail));
        item->setSizeHint(QSize(198, 320));
        if (book.id == selectedId) m_shelf->setCurrentItem(item);
    }
    const bool empty = books.isEmpty();
    m_emptyState->setVisible(empty);
    m_shelf->setVisible(!empty);
    m_shelfCount->setVisible(!empty);
    m_shelfCount->setText(empty ? QString()
                                : QString("共 %1 册 · 点击封面开始阅读").arg(books.size()));
}

void MainWindow::reloadReaderMetadata(const Book &book)
{
    m_chapters->clear();
    for (int index = 0; index < book.chapterTitles.size(); ++index)
        m_chapters->addItem(book.chapterTitles.at(index), book.chapterOffsets.value(index));
    m_chapters->setVisible(!book.chapterTitles.isEmpty());
    m_chapterLabel->setVisible(!book.chapterTitles.isEmpty());
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
    m_bookTitle->setText(book.title);
    m_reader->setPosition(book.position);
    reloadReaderMetadata(book);
    updateReaderFooter();
    m_pages->setCurrentIndex(1);
    QString error;
    m_store.savePosition(book.id, book.position, &error);
    refreshShelf();
}

void MainWindow::updateReaderFooter()
{
    if (m_currentBookId.isEmpty()) {
        m_readerFooter->setText("选择一本书开始阅读");
        return;
    }
    m_readerFooter->setText(QString("字号 %1 pt · 阅读进度自动保存").arg(m_fontSize));
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
