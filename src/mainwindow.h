#pragma once

#include "librarystore.h"

#include <QHash>
#include <QMainWindow>
#include <QPixmap>

class QComboBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QPushButton;
class QStackedWidget;
class QTimer;
class CoverGridDelegate;
class OrnamentRule;
class PaperWidget;
class ReaderWidget;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

protected:
    void closeEvent(QCloseEvent *event) override;

private:
    void refreshShelf();
    void openBook(int row);
    void saveCurrentPosition();
    void reloadReaderMetadata(const Book &book);
    void applyTheme();
    void updateReaderFooter();
    void runSyncFolder();
    void chooseSyncFolder();
    void importSyncFolder(const QString &folderPath);
    QPixmap coverFor(const Book &book);

    LibraryStore m_store;
    QListWidget *m_shelf;
    QStackedWidget *m_pages;
    ReaderWidget *m_reader;
    QTimer *m_saveTimer;
    QComboBox *m_chapters;
    QComboBox *m_bookmarks;
    QComboBox *m_annotations;
    QLineEdit *m_searchField;
    QLabel *m_bookTitle;
    QLabel *m_chapterLabel;
    QLabel *m_emptyState;
    QLabel *m_shelfCount;
    QLabel *m_readerFooter;
    QPushButton *m_themeButton;
    PaperWidget *m_shelfPage;
    PaperWidget *m_readerPage;
    OrnamentRule *m_shelfRule;
    OrnamentRule *m_readerRule;
    CoverGridDelegate *m_shelfDelegate;
    QHash<QString, QPixmap> m_coverCache;
    QString m_currentBookId;
    int m_fontSize = 18;
    bool m_darkTheme = false;
};
