#include "readerwidget.h"

#include "theme.h"

#include <QColor>
#include <QFont>
#include <QPainter>
#include <QScrollBar>
#include <QTextDocument>
#include <QTextEdit>

ReaderWidget::ReaderWidget(QWidget *parent) : QPlainTextEdit(parent)
{
    setReadOnly(true);
    setLineWrapMode(QPlainTextEdit::WidgetWidth);
    setFrameShape(QFrame::NoFrame);
    setBackgroundVisible(false);
    setFont(Theme::readerFont(18));
    setCursorWidth(2);
    document()->setDocumentMargin(34);
    setDarkTheme(false);
    connect(verticalScrollBar(), &QScrollBar::valueChanged, this, [this] { emit positionChanged(position()); });
}

void ReaderWidget::paintEvent(QPaintEvent *event)
{
    QPainter painter(viewport());
    Theme::paintReaderPaper(painter, viewport()->rect(), m_darkTheme);
    painter.end();
    QPlainTextEdit::paintEvent(event);
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
        selection.format.setBackground(Theme::annotationColor(darkTheme));
        selections.append(selection);
    }
    setExtraSelections(selections);
}

void ReaderWidget::setDarkTheme(bool enabled)
{
    if (m_darkTheme == enabled && !styleSheet().isEmpty()) {
        update();
        return;
    }
    m_darkTheme = enabled;
    setStyleSheet(Theme::readerStyleSheet(enabled));
    update();
}
