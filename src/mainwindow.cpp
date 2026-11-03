#include "mainwindow.h"

#include "annotationpanel.h"
#include "readerwidget.h"
#include "retrowidgets.h"
#include "settingsdialog.h"
#include "theme.h"

#include <QApplication>
#include <QCloseEvent>
#include <QComboBox>
#include <QDir>
#include <QEasingCurve>
#include <QEventLoop>
#include <QFile>
#include <QFileDialog>
#include <QFrame>
#include <QGraphicsDropShadowEffect>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QListWidget>
#include <QMenu>
#include <QMessageBox>
#include <QPropertyAnimation>
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
      m_pageMode(new QComboBox), m_searchField(new QLineEdit), m_bookTitle(new QLabel),
      m_chapterLabel(new QLabel), m_emptyState(new QLabel), m_shelfCount(new QLabel),
      m_readerFooter(new QLabel), m_readerHint(new QLabel), m_themeButton(new QPushButton),
      m_notesButton(new QPushButton), m_settingsButton(new QPushButton),
      m_selectionNoteButton(nullptr), m_notePanel(new AnnotationPanel),
      m_noteAnimation(nullptr), m_shelfPage(new PaperWidget), m_readerPage(new PaperWidget),
      m_shelfRule(new OrnamentRule), m_readerRule(new OrnamentRule), m_shelfDelegate(nullptr)
{
    setWindowTitle("Mark Reader");
    resize(1100, 780);
    setMinimumSize(900, 640);
    setFont(Theme::uiFont(14));
    QSettings settings;
    m_fontSize = qBound(12, settings.value("reader/fontSize", 18).toInt(), 32);
    m_darkTheme = settings.value("reader/darkTheme", false).toBool();
    m_chineseFont = settings.value("reader/chineseFont").toString();
    m_latinFont = settings.value("reader/latinFont").toString();
    const bool pageModeEnabled =
        settings.value("reader/pageMode", QStringLiteral("scroll")).toString() == QStringLiteral("page");

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
    backButton->setObjectName("backButton");
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
    m_chapters->setObjectName("chapters");
    m_chapters->setMinimumWidth(126);
    toolbar->addWidget(m_chapterLabel);
    toolbar->addWidget(m_chapters);

    m_pageMode->setObjectName("pageMode");
    m_pageMode->addItem("滚动翻页", 0);
    m_pageMode->addItem("左右翻页", 1);
    m_pageMode->setMinimumWidth(100);
    m_pageMode->setToolTip("滚动阅读或左右翻页；左右翻页模式支持方向键、空格和滚轮翻页");
    toolbar->addWidget(m_pageMode);

    auto *smaller = new QPushButton("A−");
    smaller->setToolTip("缩小字号");
    auto *larger = new QPushButton("A+");
    larger->setToolTip("放大字号");
    m_settingsButton->setText("设置");
    m_settingsButton->setObjectName("settingsButton");
    m_settingsButton->setToolTip("自定义中英文字体与字号");
    m_themeButton->setToolTip("切换夜读配色");
    m_notesButton->setText("笔记");
    m_notesButton->setObjectName("notesButton");
    m_notesButton->setCheckable(true);
    m_notesButton->setToolTip("打开读书笔记侧栏");
    toolbar->addWidget(smaller);
    toolbar->addWidget(larger);
    toolbar->addWidget(m_settingsButton);
    toolbar->addWidget(m_themeButton);
    toolbar->addWidget(m_notesButton);
    readerLayout->addLayout(toolbar);
    readerLayout->addSpacing(10);
    m_readerRule->setDarkTheme(m_darkTheme);
    readerLayout->addWidget(m_readerRule);
    readerLayout->addSpacing(8);

    auto *toolsRow = new QHBoxLayout;
    toolsRow->setSpacing(8);
    auto *annotationButton = new QPushButton("划词笔记");
    annotationButton->setObjectName("annotationButton");
    annotationButton->setToolTip("把正文中选中的文字记成一条带时间的笔记");
    auto *addBookmark = new QPushButton("添加书签");
    addBookmark->setObjectName("addBookmarkButton");
    auto *bookmarkLabel = new QLabel("书签");
    bookmarkLabel->setObjectName("toolLabel");
    m_bookmarks->setObjectName("bookmarks");
    m_bookmarks->setMinimumWidth(120);
    auto *searchLabel = new QLabel("检索");
    searchLabel->setObjectName("toolLabel");
    m_searchField->setObjectName("searchField");
    m_searchField->setPlaceholderText("在本书中检索…");
    auto *searchButton = new QPushButton("查找下一处");
    searchButton->setObjectName("searchButton");
    toolsRow->addWidget(annotationButton);
    toolsRow->addWidget(addBookmark);
    toolsRow->addWidget(bookmarkLabel);
    toolsRow->addWidget(m_bookmarks, 1);
    toolsRow->addSpacing(12);
    toolsRow->addWidget(searchLabel);
    toolsRow->addWidget(m_searchField, 1);
    toolsRow->addWidget(searchButton);
    readerLayout->addLayout(toolsRow);
    readerLayout->addSpacing(12);

    auto *readingRow = new QHBoxLayout;
    readingRow->setSpacing(10);
    auto *readerFrame = new QFrame;
    readerFrame->setObjectName("readerFrame");
    auto *frameLayout = new QVBoxLayout(readerFrame);
    frameLayout->setContentsMargins(8, 8, 8, 8);
    m_reader->setObjectName("reader");
    m_reader->setAccessibleName("正文");
    frameLayout->addWidget(m_reader);
    auto *shadow = new QGraphicsDropShadowEffect(readerFrame);
    shadow->setBlurRadius(28);
    shadow->setOffset(0, 8);
    shadow->setColor(QColor(40, 26, 12, 95));
    readerFrame->setGraphicsEffect(shadow);
    readingRow->addWidget(readerFrame, 1);

    m_notePanel->setVisible(false);
    m_notePanel->setMaximumWidth(0);
    readingRow->addWidget(m_notePanel, 0);
    readerLayout->addLayout(readingRow, 1);
    readerLayout->addSpacing(8);

    auto *footerRow = new QHBoxLayout;
    m_readerFooter->setObjectName("readerFooter");
    m_readerHint->setObjectName("readerFooter");
    footerRow->addWidget(m_readerFooter);
    footerRow->addStretch(1);
    footerRow->addWidget(m_readerHint);
    readerLayout->addLayout(footerRow);
    m_pages->addWidget(m_readerPage);
    setCentralWidget(m_pages);

    m_selectionNoteButton = new QPushButton("＋ 笔记", m_reader->viewport());
    m_selectionNoteButton->setObjectName("selectionNoteButton");
    m_selectionNoteButton->setCursor(Qt::PointingHandCursor);
    m_selectionNoteButton->setToolTip("为选中的文字记录笔记");
    m_selectionNoteButton->setVisible(false);

    m_reader->setReaderFonts(m_chineseFont, m_latinFont, m_fontSize);
    m_pageMode->setCurrentIndex(pageModeEnabled ? 1 : 0);
    m_reader->setPageMode(pageModeEnabled ? ReaderWidget::PageMode::Page
                                          : ReaderWidget::PageMode::Scroll);
    applyTheme();
    updateReaderFooter();

    m_saveTimer->setSingleShot(true);
    m_saveTimer->setInterval(700);
    connect(m_saveTimer, &QTimer::timeout, this, &MainWindow::saveCurrentPosition);
    connect(m_reader, &ReaderWidget::positionChanged, this, [this] {
        m_saveTimer->start();
        hideSelectionNoteButton();
    });
    connect(m_reader, &ReaderWidget::pageInfoChanged, this, [this] { updateReaderFooter(); });
    connect(m_reader, &ReaderWidget::selectionMade, this, &MainWindow::showSelectionNoteButton);
    connect(m_reader, &ReaderWidget::selectionCleared, this, &MainWindow::hideSelectionNoteButton);

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
    connect(syncButton, &QPushButton::clicked, this, [this] { runSyncFolder(); });
    syncButton->setToolTip("从同步文件夹导入新书并自动去重；右键可更换文件夹");
    syncButton->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(syncButton, &QWidget::customContextMenuRequested, this,
            [this, syncButton](const QPoint &position) {
                QMenu menu(syncButton);
                QAction *importAction = menu.addAction("立即导入");
                QAction *chooseAction = menu.addAction("更换同步文件夹…");
                QAction *chosen = menu.exec(syncButton->mapToGlobal(position));
                if (chosen == importAction) runSyncFolder();
                else if (chosen == chooseAction) chooseSyncFolder();
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
        hideSelectionNoteButton();
        m_pages->setCurrentIndex(0);
    });
    connect(smaller, &QPushButton::clicked, this, [this] {
        m_fontSize = qMax(12, m_fontSize - 1);
        applyReaderFonts();
        QSettings().setValue("reader/fontSize", m_fontSize);
        updateReaderFooter();
    });
    connect(larger, &QPushButton::clicked, this, [this] {
        m_fontSize = qMin(32, m_fontSize + 1);
        applyReaderFonts();
        QSettings().setValue("reader/fontSize", m_fontSize);
        updateReaderFooter();
    });
    connect(m_settingsButton, &QPushButton::clicked, this, [this] { openSettings(); });
    connect(m_themeButton, &QPushButton::clicked, this, [this] {
        m_darkTheme = !m_darkTheme;
        QSettings().setValue("reader/darkTheme", m_darkTheme);
        applyTheme();
    });
    connect(m_pageMode, qOverload<int>(&QComboBox::activated), this, [this](int index) {
        const bool page = index == 1;
        m_reader->setPageMode(page ? ReaderWidget::PageMode::Page : ReaderWidget::PageMode::Scroll);
        QSettings().setValue("reader/pageMode", page ? "page" : "scroll");
        updateReaderFooter();
        m_reader->setFocus();
    });
    connect(m_notesButton, &QPushButton::toggled, this, [this](bool checked) {
        setNotePanelVisible(checked);
    });
    connect(m_selectionNoteButton, &QPushButton::clicked, this, [this] { beginAnnotation(); });
    connect(annotationButton, &QPushButton::clicked, this, [this] { beginAnnotation(); });
    connect(m_notePanel, &AnnotationPanel::closeRequested, this, [this] {
        m_notesButton->setChecked(false);
    });
    connect(m_notePanel, &AnnotationPanel::saveRequested, this,
            [this](int, int, const QString &, const QString &) { saveAnnotationFromPanel(); });
    connect(m_notePanel, &AnnotationPanel::annotationActivated, this, [this](int start, int end) {
        m_reader->setPosition(start);
        m_reader->selectRange(start, end);
        m_reader->setFocus();
        hideSelectionNoteButton();
    });
    connect(m_chapters, qOverload<int>(&QComboBox::activated), this, [this](int index) {
        if (index >= 0) m_reader->setPosition(m_chapters->itemData(index).toInt());
    });
    connect(m_bookmarks, qOverload<int>(&QComboBox::activated), this, [this](int index) {
        if (index >= 0) m_reader->setPosition(m_bookmarks->itemData(index).toInt());
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
    m_notePanel->update();
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

void MainWindow::refreshAnnotationPanel()
{
    for (const Book &book : m_store.books()) {
        if (book.id != m_currentBookId) continue;
        m_notePanel->setAnnotations(book.annotations);
        m_notesButton->setText(book.annotations.isEmpty()
                                   ? QStringLiteral("笔记")
                                   : QStringLiteral("笔记 %1").arg(book.annotations.size()));
        return;
    }
    m_notePanel->setAnnotations({});
    m_notesButton->setText("笔记");
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
    m_reader->showAnnotations(book.annotations, m_darkTheme);
    refreshAnnotationPanel();
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
    hideSelectionNoteButton();
    m_notePanel->clearComposer();
    m_currentBookId = book.id;
    m_reader->setPlainText(text);
    m_reader->setReaderFonts(m_chineseFont, m_latinFont, m_fontSize);
    m_bookTitle->setText(book.title);
    m_reader->setPosition(book.position);
    reloadReaderMetadata(book);
    updateReaderFooter();
    m_pages->setCurrentIndex(1);
    m_reader->setFocus();
    QString error;
    m_store.savePosition(book.id, book.position, &error);
    refreshShelf();
}

void MainWindow::updateReaderFooter()
{
    if (m_currentBookId.isEmpty()) {
        m_readerFooter->setText("选择一本书开始阅读");
        m_readerHint->setText("滚轮阅读 · 自动保存进度");
        return;
    }
    const QString chinese = m_chineseFont.isEmpty() ? QStringLiteral("默认中文") : m_chineseFont;
    const QString latin = m_latinFont.isEmpty() ? QStringLiteral("默认英文") : m_latinFont;
    QString text = QString("字号 %1 pt · %2 / %3").arg(m_fontSize).arg(chinese, latin);
    if (m_reader->pageMode() == ReaderWidget::PageMode::Page) {
        text += QString(" · 第 %1 / %2 页").arg(m_reader->currentPage()).arg(m_reader->pageCount());
        m_readerHint->setText("← / → 或滚轮翻页 · 自动保存进度");
    } else {
        m_readerHint->setText("滚轮阅读 · 自动保存进度");
    }
    m_readerFooter->setText(text);
}

void MainWindow::applyReaderFonts()
{
    const int saved = m_reader->position();
    m_reader->setReaderFonts(m_chineseFont, m_latinFont, m_fontSize);
    if (!m_currentBookId.isEmpty()) m_reader->setPosition(saved);
    updateReaderFooter();
}

void MainWindow::openSettings()
{
    SettingsDialog dialog(this);
    dialog.setChineseFont(m_chineseFont);
    dialog.setLatinFont(m_latinFont);
    dialog.setPointSize(m_fontSize);
    if (dialog.exec() != QDialog::Accepted) return;
    m_chineseFont = dialog.chineseFont();
    m_latinFont = dialog.latinFont();
    m_fontSize = qBound(12, dialog.pointSize(), 32);
    QSettings settings;
    settings.setValue("reader/chineseFont", m_chineseFont);
    settings.setValue("reader/latinFont", m_latinFont);
    settings.setValue("reader/fontSize", m_fontSize);
    applyReaderFonts();
}

void MainWindow::setNotePanelVisible(bool visible)
{
    if (m_notesButton->isChecked() != visible) m_notesButton->setChecked(visible);
    if (!m_noteAnimation) {
        m_noteAnimation = new QPropertyAnimation(m_notePanel, "maximumWidth", this);
        m_noteAnimation->setDuration(220);
        m_noteAnimation->setEasingCurve(QEasingCurve::OutCubic);
        connect(m_noteAnimation, &QPropertyAnimation::finished, this, [this] {
            if (m_noteAnimation->endValue().toInt() <= 0) m_notePanel->setVisible(false);
        });
    }
    m_noteAnimation->stop();
    if (visible) {
        m_notePanel->setVisible(true);
        hideSelectionNoteButton();
        if (!m_reader->selectedText().isEmpty()) {
            QString quote = m_reader->selectedText();
            quote.replace(QChar::ParagraphSeparator, '\n');
            quote.replace(QChar::LineSeparator, '\n');
            m_notePanel->setSelection(m_reader->selectionStart(), m_reader->selectionEnd(), quote);
        }
    } else {
        hideSelectionNoteButton();
    }
    m_noteAnimation->setStartValue(m_notePanel->maximumWidth());
    m_noteAnimation->setEndValue(visible ? 340 : 0);
    m_noteAnimation->start();
}

void MainWindow::beginAnnotation()
{
    if (m_currentBookId.isEmpty()) return;
    QString quote = m_reader->selectedText();
    quote.replace(QChar::ParagraphSeparator, '\n');
    quote.replace(QChar::LineSeparator, '\n');
    if (quote.trimmed().isEmpty()) {
        statusBar()->showMessage("请先在正文中划选需要记录的文字。", 4000);
        return;
    }
    const int start = m_reader->selectionStart();
    const int end = m_reader->selectionEnd();
    m_notePanel->setSelection(start, end, quote);
    if (!m_notesButton->isChecked()) m_notesButton->setChecked(true);
    else setNotePanelVisible(true);
    m_notePanel->focusComposer();
    hideSelectionNoteButton();
}

void MainWindow::saveAnnotationFromPanel()
{
    if (m_currentBookId.isEmpty()) return;
    const int start = m_notePanel->composerStart();
    const int end = m_notePanel->composerEnd();
    if (start < 0 || end <= start) return;
    QString error;
    if (!m_store.addAnnotation(m_currentBookId, start, end, m_notePanel->composerQuote(),
                               m_notePanel->composerNote(), &error)) {
        QMessageBox::warning(this, "无法添加笔记", error);
        return;
    }
    for (const Book &book : m_store.books()) if (book.id == m_currentBookId) reloadReaderMetadata(book);
    m_notePanel->clearComposer();
    m_reader->setFocus();
    statusBar()->showMessage("已记录笔记，带时间戳保存。", 3000);
}

void MainWindow::showSelectionNoteButton(const QString &text, int, int, const QRect &anchor)
{
    if (text.trimmed().isEmpty() || m_currentBookId.isEmpty()) {
        hideSelectionNoteButton();
        return;
    }
    if (m_notePanel->isVisible()) {
        m_notePanel->setSelection(m_reader->selectionStart(), m_reader->selectionEnd(), text);
        return;
    }
    m_selectionNoteButton->setToolTip(QString("记录：%1").arg(text.left(24)));
    m_selectionNoteButton->adjustSize();
    QPoint position = anchor.bottomRight() + QPoint(8, 6);
    const int maxX = m_reader->viewport()->width() - m_selectionNoteButton->width() - 6;
    const int maxY = m_reader->viewport()->height() - m_selectionNoteButton->height() - 6;
    position.setX(qBound(6, position.x(), qMax(6, maxX)));
    position.setY(qBound(6, position.y(), qMax(6, maxY)));
    m_selectionNoteButton->move(position);
    m_selectionNoteButton->show();
    m_selectionNoteButton->raise();
}

void MainWindow::hideSelectionNoteButton()
{
    if (m_selectionNoteButton) m_selectionNoteButton->hide();
}

void MainWindow::saveCurrentPosition()
{
    if (m_currentBookId.isEmpty()) return;
    QString error;
    if (!m_store.savePosition(m_currentBookId, m_reader->position(), &error))
        statusBar()->showMessage("阅读进度保存失败：" + error, 4000);
    refreshShelf();
}

void MainWindow::runSyncFolder()
{
    QSettings settings;
    QString folder = settings.value("sync/sourceFolder").toString();
    if (folder.isEmpty()) folder = LibraryStore::defaultSyncFolder();
    while (!QDir(folder).exists()) {
        folder = QFileDialog::getExistingDirectory(this, "选择同步文件夹", folder);
        if (folder.isEmpty()) {
            statusBar()->showMessage("尚未设置同步文件夹，请先选择一个书籍文件夹。", 5000);
            return;
        }
    }
    settings.setValue("sync/sourceFolder", folder);
    importSyncFolder(folder);
}

void MainWindow::chooseSyncFolder()
{
    const QString current =
        QSettings().value("sync/sourceFolder", LibraryStore::defaultSyncFolder()).toString();
    const QString folder = QFileDialog::getExistingDirectory(this, "选择同步文件夹", current);
    if (folder.isEmpty()) return;
    QSettings().setValue("sync/sourceFolder", folder);
    statusBar()->showMessage(QString("同步文件夹已设为：%1").arg(folder), 5000);
}

void MainWindow::importSyncFolder(const QString &folderPath)
{
    FolderImportSummary summary;
    QString error;
    statusBar()->showMessage(QString("正在从同步文件夹导入：%1…").arg(folderPath));
    QApplication::setOverrideCursor(Qt::WaitCursor);
    QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
    const bool imported = m_store.importFolder(folderPath, &summary, &error);
    QApplication::restoreOverrideCursor();
    if (!imported) {
        QMessageBox::warning(this, "同步失败", error);
        return;
    }
    refreshShelf();
    QString message = summary.imported > 0
        ? QString("已从同步文件夹导入 %1 本").arg(summary.imported)
        : QString("同步文件夹中没有新书");
    if (summary.skippedDuplicates > 0)
        message += QString("，跳过重复 %1 本").arg(summary.skippedDuplicates);
    if (summary.removedDuplicates > 0)
        message += QString("，合并重复 %1 本").arg(summary.removedDuplicates);
    statusBar()->showMessage(message, 7000);
    if (summary.failed > 0) {
        QStringList details = summary.warnings.mid(0, 5);
        if (summary.warnings.size() > details.size())
            details.append(QString("…另有 %1 个文件未能导入")
                               .arg(summary.warnings.size() - details.size()));
        QMessageBox::warning(this, "部分文件未能导入",
                             QString("%1 个文件未能导入：\n\n%2")
                                 .arg(summary.failed)
                                 .arg(details.join("\n")));
    }
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    saveCurrentPosition();
    event->accept();
}
