#pragma once

#include <QPlainTextEdit>

class ReaderWidget : public QPlainTextEdit {
    Q_OBJECT
public:
    explicit ReaderWidget(QWidget *parent = nullptr);
    int position() const;
    void setPosition(int position);
    void setPointSize(int pointSize);

signals:
    void positionChanged(int position);
};
