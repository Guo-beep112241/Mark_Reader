#pragma once

#include "librarystore.h"

#include <QMainWindow>

class QListWidget;
class QStackedWidget;
class ReaderWidget;
class QTimer;
class QComboBox;
class QLineEdit;

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
    LibraryStore m_store;
    QListWidget *m_shelf;
    QStackedWidget *m_pages;
    ReaderWidget *m_reader;
    QTimer *m_saveTimer;
    QComboBox *m_chapters;
    QComboBox *m_bookmarks;
    QComboBox *m_annotations;
    QLineEdit *m_searchField;
    QString m_currentBookId;
    int m_fontSize = 18;
    bool m_darkTheme = false;
};
