#include "readerwidget.h"

#include "theme.h"

#include <QColor>
#include <QEasingCurve>
#include <QFont>
#include <QKeyEvent>
#include <QLinearGradient>
#include <QMouseEvent>
#include <QPainter>
#include <QScrollBar>
#include <QSignalBlocker>
#include <QTextBlock>
#include <QTextDocument>
#include <QTextEdit>
#include <QTimer>
#include <QVariantAnimation>
#include <QWheelEvent>

namespace {
// 中文、日文、韩文和中文标点都由中文字体渲染；其余字符交给英文字体。
// 这样不必给每个字符建格式，整段中文只保留一层基础格式，切换英文时只覆盖英文片段。
bool isCjkCharacter(QChar character)
{
    const ushort code = character.unicode();
    return (code >= 0x2E80 && code <= 0x9FFF)
        || (code >= 0xAC00 && code <= 0xD7AF)
        || (code >= 0xF900 && code <= 0xFAFF)
        || (code >= 0xFE30 && code <= 0xFE4F)
        || (code >= 0xFF00 && code <= 0xFFEF);
}
}

ReaderWidget::ReaderWidget(QWidget *parent) : QPlainTextEdit(parent)
{
    setReadOnly(true);
    setLineWrapMode(QPlainTextEdit::WidgetWidth);
    setFrameShape(QFrame::NoFrame);
    setBackgroundVisible(false);
    setFocusPolicy(Qt::StrongFocus);
    setFont(Theme::readerFont(18));
    setCursorWidth(2);
    document()->setDocumentMargin(34);
    setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    setDarkTheme(false);
    connect(verticalScrollBar(), &QScrollBar::valueChanged, this, [this] {
        emit positionChanged(position());
        emitPageInfo();
    });
}

void ReaderWidget::paintEvent(QPaintEvent *event)
{
    QPainter painter(viewport());
    Theme::paintReaderPaper(painter, viewport()->rect(), m_darkTheme);
    painter.end();

    if (m_turning && !m_flipFrom.isNull() && !m_flipTo.isNull()) {
        QPainter overlay(viewport());
        overlay.setRenderHint(QPainter::SmoothPixmapTransform, true);
        const int width = viewport()->width();
        const int height = viewport()->height();
        const int travel = qRound(qBound(0.0, m_flipProgress, 1.0) * width);
        int seam = 0;
        if (m_flipDirection >= 0) {
            overlay.drawPixmap(-travel, 0, m_flipFrom);
            overlay.drawPixmap(width - travel, 0, m_flipTo);
            seam = width - travel;
        } else {
            overlay.drawPixmap(travel, 0, m_flipFrom);
            overlay.drawPixmap(-width + travel, 0, m_flipTo);
            seam = travel;
        }
        // 书页边缘的柔和阴影和一条烫金细线，让横向滑动更像翻动实体书页。
        QLinearGradient shade(seam - 26, 0, seam + 26, 0);
        shade.setColorAt(0.0, QColor(0, 0, 0, 0));
        shade.setColorAt(0.5, QColor(30, 18, 8, 80));
        shade.setColorAt(1.0, QColor(0, 0, 0, 0));
        overlay.fillRect(QRect(seam - 26, 0, 52, height), shade);
        QColor edge = Theme::palette(m_darkTheme).gold;
        edge.setAlpha(120);
        overlay.setPen(edge);
        overlay.drawLine(seam, 0, seam, height);
        return;
    }

    QPlainTextEdit::paintEvent(event);
}

int ReaderWidget::position() const
{
    // 直接用第一个可见文本块，避免滚动后布局尚未刷新时 cursorForPosition 取到旧位置。
    return firstVisibleBlock().position();
}

void ReaderWidget::setPosition(int position)
{
    cancelPageTurn();
    QTextCursor cursor = textCursor();
    cursor.setPosition(qBound(0, position, document()->characterCount() - 1));
    setTextCursor(cursor);
    ensureCursorVisible();
    if (m_pageMode == PageMode::Page) alignPageToPosition();
    emitPageInfo();
}

void ReaderWidget::setPointSize(int pointSize)
{
    setReaderFonts(m_chineseFamily, m_latinFamily, pointSize);
}

void ReaderWidget::setReaderFonts(const QString &chineseFamily, const QString &latinFamily, int pointSize)
{
    const int savedPosition = position();
    m_chineseFamily = chineseFamily;
    m_latinFamily = latinFamily;
    m_pointSize = qBound(6, pointSize, 72);
    applyFonts();
    if (!document()->isEmpty()) setPosition(savedPosition);
    emitPageInfo();
    update();
}

void ReaderWidget::applyFonts()
{
    const QString chinese = m_chineseFamily.isEmpty() ? Theme::defaultChineseFamily() : m_chineseFamily;
    const QString latin = m_latinFamily.isEmpty() ? Theme::defaultLatinFamily() : m_latinFamily;

    QFont baseFont(chinese);
    baseFont.setPointSize(m_pointSize);
    baseFont.setStyleStrategy(QFont::PreferAntialias);
    setFont(baseFont);
    if (document()->isEmpty()) return;

    const int savedPosition = textCursor().position();
    const int savedAnchor = textCursor().anchor();
    const int savedScroll = verticalScrollBar()->value();
    {
        QTextCursor cursor(document());
        cursor.select(QTextCursor::Document);
        QTextCharFormat format;
        format.setFont(baseFont);
        cursor.setCharFormat(format);
    }
    if (!latin.isEmpty() && latin != chinese) applyLatinFont(latin);

    QTextCursor restored(document());
    restored.setPosition(savedAnchor);
    restored.setPosition(savedPosition, QTextCursor::KeepAnchor);
    setTextCursor(restored);
    verticalScrollBar()->setValue(qMin(savedScroll, verticalScrollBar()->maximum()));
}

void ReaderWidget::applyLatinFont(const QString &latinFamily)
{
    QFont latinFont(latinFamily);
    latinFont.setPointSize(m_pointSize);
    QTextCharFormat latinFormat;
    latinFormat.setFont(latinFont);

    QTextCursor cursor(document());
    for (QTextBlock block = document()->begin(); block.isValid(); block = block.next()) {
        const QString text = block.text();
        int runStart = -1;
        for (int index = 0; index <= text.size(); ++index) {
            const bool latin = index < text.size() && !isCjkCharacter(text.at(index));
            if (latin && runStart < 0) {
                runStart = index;
            } else if (!latin && runStart >= 0) {
                cursor.setPosition(block.position() + runStart);
                cursor.setPosition(block.position() + index, QTextCursor::KeepAnchor);
                cursor.mergeCharFormat(latinFormat);
                runStart = -1;
            }
        }
    }
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

void ReaderWidget::setPageMode(PageMode mode)
{
    if (m_pageMode == mode) {
        emitPageInfo();
        return;
    }
    cancelPageTurn();
    m_pageMode = mode;
    m_wheelAccumulator = 0;
    setVerticalScrollBarPolicy(mode == PageMode::Page ? Qt::ScrollBarAlwaysOff
                                                      : Qt::ScrollBarAsNeeded);
    if (mode == PageMode::Page) {
        const int keep = position();
        QTimer::singleShot(0, this, [this, keep] {
            if (m_pageMode != PageMode::Page) return;
            setPosition(keep);
            update();
        });
    }
    emitPageInfo();
    update();
}

int ReaderWidget::currentPage() const
{
    if (m_pageMode != PageMode::Page) return 1;
    // QPlainTextEdit 的纵向滚动条以“行”为单位，pageStep 就是当前视口能容纳的行数。
    const int step = qMax(1, verticalScrollBar()->pageStep());
    const int pages = pageCount();
    const int value = verticalScrollBar()->value();
    const int maximum = verticalScrollBar()->maximum();
    // 最后一页的起点是 maximum，通常不是 pageStep 的整数倍，需要单独判断。
    if (maximum > 0 && value >= maximum) return pages;
    return qBound(1, value / step + 1, pages);
}

int ReaderWidget::pageCount() const
{
    if (m_pageMode != PageMode::Page) return 1;
    const int step = qMax(1, verticalScrollBar()->pageStep());
    const int totalLines = verticalScrollBar()->maximum() + step;
    return qMax(1, (totalLines + step - 1) / step);
}

void ReaderWidget::nextPage()
{
    turnPage(1);
}

void ReaderWidget::previousPage()
{
    turnPage(-1);
}

void ReaderWidget::cancelPageTurn()
{
    if (m_pageAnimation) m_pageAnimation->stop();
    m_turning = false;
    m_pendingPage = -1;
    m_flipProgress = 0.0;
    m_flipFrom = {};
    m_flipTo = {};
}

void ReaderWidget::turnPage(int direction)
{
    if (m_pageMode != PageMode::Page || m_turning) return;
    QScrollBar *bar = verticalScrollBar();
    const int step = qMax(1, bar->pageStep());
    const int current = bar->value();
    const int target = direction > 0 ? qMin(current + step, bar->maximum())
                                     : qMax(current - step, 0);
    if (target == current) return;

    if (!m_animationsEnabled) {
        bar->setValue(target);
        emitPageInfo();
        return;
    }

    m_flipFrom = captureViewport();
    {
        QSignalBlocker blocker(bar);
        bar->setValue(target);
        m_flipTo = captureViewport();
        bar->setValue(current);
    }
    if (m_flipFrom.isNull() || m_flipTo.isNull()) {
        bar->setValue(target);
        emitPageInfo();
        return;
    }

    m_flipDirection = direction;
    m_flipProgress = 0.0;
    m_pendingPage = target;
    m_turning = true;
    if (!m_pageAnimation) {
        m_pageAnimation = new QVariantAnimation(this);
        m_pageAnimation->setDuration(360);
        m_pageAnimation->setEasingCurve(QEasingCurve::OutCubic);
        connect(m_pageAnimation, &QVariantAnimation::valueChanged, this,
                [this](const QVariant &value) {
                    m_flipProgress = value.toReal();
                    update();
                });
        connect(m_pageAnimation, &QVariantAnimation::finished, this,
                &ReaderWidget::finishPageTurn);
    }
    m_pageAnimation->stop();
    m_pageAnimation->setStartValue(0.0);
    m_pageAnimation->setEndValue(1.0);
    m_pageAnimation->start();
    update();
}

void ReaderWidget::finishPageTurn()
{
    if (!m_turning) return;
    m_turning = false;
    const int target = m_pendingPage;
    m_pendingPage = -1;
    m_flipFrom = {};
    m_flipTo = {};
    if (target >= 0) verticalScrollBar()->setValue(target);
    update();
    emitPageInfo();
    emit positionChanged(position());
}

QPixmap ReaderWidget::captureViewport()
{
    QPixmap pixmap(viewport()->size());
    pixmap.fill(Qt::transparent);
    // 只画正文本身，避免把浮在视口里的“记笔记”按钮一起截进翻页动画。
    viewport()->render(&pixmap, QPoint(), QRegion(), QWidget::DrawWindowBackground);
    return pixmap;
}

void ReaderWidget::emitPageInfo()
{
    emit pageInfoChanged(currentPage(), pageCount());
}

void ReaderWidget::keyPressEvent(QKeyEvent *event)
{
    if (m_pageMode == PageMode::Page && !(event->modifiers() & Qt::ShiftModifier)) {
        switch (event->key()) {
        case Qt::Key_Right:
        case Qt::Key_Down:
        case Qt::Key_PageDown:
        case Qt::Key_Space:
            nextPage();
            event->accept();
            return;
        case Qt::Key_Left:
        case Qt::Key_Up:
        case Qt::Key_PageUp:
            previousPage();
            event->accept();
            return;
        case Qt::Key_Home:
            setPosition(0);
            event->accept();
            return;
        case Qt::Key_End:
            setPosition(document()->characterCount() - 1);
            event->accept();
            return;
        default:
            break;
        }
    }
    QPlainTextEdit::keyPressEvent(event);
}

void ReaderWidget::wheelEvent(QWheelEvent *event)
{
    if (m_pageMode != PageMode::Page) {
        QPlainTextEdit::wheelEvent(event);
        return;
    }
    const int delta = event->angleDelta().y();
    if (delta == 0) {
        event->accept();
        return;
    }
    m_wheelAccumulator += delta;
    if (qAbs(m_wheelAccumulator) >= 120) {
        turnPage(m_wheelAccumulator < 0 ? 1 : -1);
        m_wheelAccumulator = 0;
    }
    event->accept();
}

void ReaderWidget::mouseReleaseEvent(QMouseEvent *event)
{
    QPlainTextEdit::mouseReleaseEvent(event);
    if (event->button() != Qt::LeftButton) return;

    QString text = textCursor().selectedText();
    if (!text.isEmpty()) {
        text.replace(QChar::ParagraphSeparator, '\n');
        text.replace(QChar::LineSeparator, '\n');
        emit selectionMade(text, selectionStart(), selectionEnd(), cursorRect(textCursor()));
        return;
    }
    emit selectionCleared();
    if (m_pageMode != PageMode::Page) return;

    const int edge = qMax(36, viewport()->width() / 8);
    const int x = event->position().toPoint().x();
    if (x < edge) turnPage(-1);
    else if (x > viewport()->width() - edge) turnPage(1);
}

void ReaderWidget::resizeEvent(QResizeEvent *event)
{
    QPlainTextEdit::resizeEvent(event);
    if (m_pageMode == PageMode::Page) {
        QTimer::singleShot(0, this, [this] {
            if (m_pageMode == PageMode::Page) emitPageInfo();
        });
    }
}

void ReaderWidget::alignPageToPosition()
{
    // 滚动条单位是行，因此把光标在视口里的像素位置换算成行偏移。
    const QRect rect = cursorRect(textCursor());
    const int lineHeight = qMax(1, rect.height());
    const int delta = qRound((rect.top() - document()->documentMargin()) / qreal(lineHeight));
    const int target = verticalScrollBar()->value() + delta;
    verticalScrollBar()->setValue(qBound(0, target, verticalScrollBar()->maximum()));
}
