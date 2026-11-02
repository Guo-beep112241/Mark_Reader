#pragma once

#include "book.h"

#include <QPlainTextEdit>

class ReaderWidget : public QPlainTextEdit {
    Q_OBJECT
public:
    explicit ReaderWidget(QWidget *parent = nullptr);
    int position() const;
    void setPosition(int position);
    void setPointSize(int pointSize);
    bool findText(const QString &text);
    void selectRange(int start, int end);
    void showAnnotations(const QList<Annotation> &annotations, bool darkTheme);
    QString selectedText() const;
    int selectionStart() const;
    int selectionEnd() const;
    void setDarkTheme(bool enabled);
    bool darkTheme() const { return m_darkTheme; }

protected:
    void paintEvent(QPaintEvent *event) override;

signals:
    void positionChanged(int position);

private:
    bool m_darkTheme = false;
};
