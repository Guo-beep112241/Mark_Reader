#include "readerwidget.h"

#include <QColor>
#include <QTextDocument>
#include <QTextEdit>
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

bool ReaderWidget::findText(const QString &text)
{
    if (text.isEmpty()) return false;
    if (find(text)) return true;
    moveCursor(QTextCursor::Start);
    return find(text);
}

void ReaderWidget::selectRange(int start, int end)
{
    QTextCursor cursor(document());
    const int maximum = document()->characterCount() - 1;
    cursor.setPosition(qBound(0, start, maximum));
    cursor.setPosition(qBound(0, end, maximum), QTextCursor::KeepAnchor);
    setTextCursor(cursor);
    ensureCursorVisible();
}

QString ReaderWidget::selectedText() const { return textCursor().selectedText(); }
int ReaderWidget::selectionStart() const { return textCursor().selectionStart(); }
int ReaderWidget::selectionEnd() const { return textCursor().selectionEnd(); }

void ReaderWidget::showAnnotations(const QList<Annotation> &annotations, bool darkTheme)
{
    QList<QTextEdit::ExtraSelection> selections;
    for (const Annotation &annotation : annotations) {
        if (annotation.end <= annotation.start) continue;
        QTextEdit::ExtraSelection selection;
        selection.cursor = QTextCursor(document());
        selection.cursor.setPosition(qBound(0, annotation.start, document()->characterCount() - 1));
        selection.cursor.setPosition(qBound(0, annotation.end, document()->characterCount() - 1), QTextCursor::KeepAnchor);
        selection.format.setBackground(darkTheme ? QColor("#685b25") : QColor("#fff0a3"));
        selections.append(selection);
    }
    setExtraSelections(selections);
}

void ReaderWidget::setDarkTheme(bool enabled)
{
    setStyleSheet(enabled
        ? "QPlainTextEdit { background: #22252a; color: #e5e0d6; border: 0; padding: 28px; }"
        : "QPlainTextEdit { background: #fbf8f0; color: #302c25; border: 0; padding: 28px; }");
}
