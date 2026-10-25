#pragma once

#include "librarystore.h"

#include <QMainWindow>

class QListWidget;
class QStackedWidget;
class ReaderWidget;
class QTimer;

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
    LibraryStore m_store;
    QListWidget *m_shelf;
    QStackedWidget *m_pages;
    ReaderWidget *m_reader;
    QTimer *m_saveTimer;
    QString m_currentBookId;
    int m_fontSize = 18;
};
