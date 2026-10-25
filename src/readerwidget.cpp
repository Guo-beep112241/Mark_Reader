#include "readerwidget.h"

#include <QFont>
#include <QScrollBar>

ReaderWidget::ReaderWidget(QWidget *parent) : QPlainTextEdit(parent)
{
    setReadOnly(true);
    setLineWrapMode(QPlainTextEdit::WidgetWidth);
    setStyleSheet("QPlainTextEdit { background: #fbf8f0; color: #302c25; border: 0; padding: 28px; }");
    connect(verticalScrollBar(), &QScrollBar::valueChanged, this, [this] { emit positionChanged(position()); });
}

int ReaderWidget::position() const
{
    return cursorForPosition(QPoint(0, 0)).position();
}

void ReaderWidget::setPosition(int position)
{
    QTextCursor cursor = textCursor();
    cursor.setPosition(qBound(0, position, document()->characterCount() - 1));
    setTextCursor(cursor);
    ensureCursorVisible();
}

void ReaderWidget::setPointSize(int pointSize)
{
    QFont current = font();
    current.setPointSize(pointSize);
    setFont(current);
}
