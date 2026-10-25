#include "mainwindow.h"
#include "readerwidget.h"

#include <QCloseEvent>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QSettings>
#include <QSlider>
#include <QStatusBar>
#include <QStandardPaths>
#include <QStackedWidget>
#include <QTimer>
#include <QVBoxLayout>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent),
      m_store(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)),
      m_shelf(new QListWidget), m_pages(new QStackedWidget), m_reader(new ReaderWidget),
      m_saveTimer(new QTimer(this))
{
    setWindowTitle("Mark Reader");
    resize(900, 680);
    QSettings settings;
    m_fontSize = settings.value("reader/fontSize", 18).toInt();

    auto *shelfPage = new QWidget;
    auto *shelfLayout = new QVBoxLayout(shelfPage);
    auto *shelfHeader = new QHBoxLayout;
    auto *title = new QLabel("我的书架");
    title->setStyleSheet("font-size: 26px; font-weight: 600;");
    auto *importButton = new QPushButton("导入 TXT");
    shelfHeader->addWidget(title);
    shelfHeader->addStretch();
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
    toolbar->addWidget(backButton);
    toolbar->addWidget(bookTitle, 1);
    toolbar->addWidget(smaller);
    toolbar->addWidget(larger);
    readerLayout->addLayout(toolbar);
    readerLayout->addWidget(m_reader);
    m_pages->addWidget(readerPage);
    setCentralWidget(m_pages);

    m_reader->setPointSize(m_fontSize);
    m_saveTimer->setSingleShot(true);
    m_saveTimer->setInterval(700);
    connect(m_saveTimer, &QTimer::timeout, this, &MainWindow::saveCurrentPosition);
    connect(m_reader, &ReaderWidget::positionChanged, m_saveTimer, qOverload<>(&QTimer::start));
    connect(importButton, &QPushButton::clicked, this, [this] {
        const QString path = QFileDialog::getOpenFileName(this, "导入 TXT", {}, "文本文件 (*.txt)");
        if (path.isEmpty()) return;
        QString error;
        if (m_store.importTextFile(path, &error).isEmpty()) QMessageBox::warning(this, "导入失败", error);
        refreshShelf();
    });
    connect(m_shelf, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem *) { openBook(m_shelf->currentRow()); });
    connect(backButton, &QPushButton::clicked, this, [this] { saveCurrentPosition(); m_pages->setCurrentIndex(0); });
    connect(smaller, &QPushButton::clicked, this, [this] {
        m_fontSize = qMax(12, m_fontSize - 1); m_reader->setPointSize(m_fontSize);
        QSettings().setValue("reader/fontSize", m_fontSize);
    });
    connect(larger, &QPushButton::clicked, this, [this] {
        m_fontSize = qMin(32, m_fontSize + 1); m_reader->setPointSize(m_fontSize);
        QSettings().setValue("reader/fontSize", m_fontSize);
    });
    refreshShelf();
}

MainWindow::~MainWindow() { saveCurrentPosition(); }

void MainWindow::refreshShelf()
{
    m_shelf->clear();
    for (const Book &book : m_store.books()) {
        const QString progress = book.position > 0 ? QString(" · 已读到 %1 字").arg(book.position) : " · 尚未阅读";
        m_shelf->addItem(book.title + progress);
    }
}

void MainWindow::openBook(int row)
{
    const auto books = m_store.books();
    if (row < 0 || row >= books.size()) return;
    const Book book = books.at(row);
    QFile file(m_store.textPath(book));
    if (!file.open(QIODevice::ReadOnly)) {
        QMessageBox::warning(this, "无法打开", "找不到书籍副本，请重新导入。");
        return;
    }
    m_currentBookId = book.id;
    m_reader->setPlainText(QString::fromUtf8(file.readAll()));
    findChild<QLabel *>("bookTitle")->setText(book.title);
    m_reader->setPosition(book.position);
    m_pages->setCurrentIndex(1);
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
