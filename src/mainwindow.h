#pragma once

#include "librarystore.h"

#include <QHash>
#include <QMainWindow>
#include <QPixmap>

class QComboBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QPropertyAnimation;
class QPushButton;
class QStackedWidget;
class QTimer;
class AnnotationPanel;
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
    void removeSelectedBook();
    void saveCurrentPosition();
    void reloadReaderMetadata(const Book &book);
    void applyTheme();
    void updateReaderFooter();
    void runSyncFolder();
    void chooseSyncFolder();
    void importSyncFolder(const QString &folderPath);
    QPixmap coverFor(const Book &book);

    void openSettings();
    void applyReaderFonts();
    void setNotePanelVisible(bool visible);
    void refreshAnnotationPanel();
    void beginAnnotation();
    void saveAnnotationFromPanel();
    void showSelectionNoteButton(const QString &text, int start, int end, const QRect &anchor);
    void hideSelectionNoteButton();

    LibraryStore m_store;
    QListWidget *m_shelf;
    QStackedWidget *m_pages;
    ReaderWidget *m_reader;
    QTimer *m_saveTimer;
    QComboBox *m_chapters;
    QComboBox *m_bookmarks;
    QComboBox *m_pageMode;
    QLineEdit *m_searchField;
    QLabel *m_bookTitle;
    QLabel *m_chapterLabel;
    QLabel *m_emptyState;
    QLabel *m_shelfCount;
    QLabel *m_readerFooter;
    QLabel *m_readerHint;
    QPushButton *m_themeButton;
    QPushButton *m_openButton = nullptr;
    QPushButton *m_removeButton = nullptr;
    QPushButton *m_notesButton;
    QPushButton *m_settingsButton;
    QPushButton *m_selectionNoteButton;
    AnnotationPanel *m_notePanel;
    QPropertyAnimation *m_noteAnimation;
    PaperWidget *m_shelfPage;
    PaperWidget *m_readerPage;
    OrnamentRule *m_shelfRule;
    OrnamentRule *m_readerRule;
    CoverGridDelegate *m_shelfDelegate;
    QHash<QString, QPixmap> m_coverCache;
    QString m_currentBookId;
    QString m_chineseFont;
    QString m_latinFont;
    int m_fontSize = 18;
    bool m_darkTheme = false;
};
