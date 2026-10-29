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

signals:
    void positionChanged(int position);
};
