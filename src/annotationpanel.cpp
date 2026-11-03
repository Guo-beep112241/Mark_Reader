#include "annotationpanel.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>

#include <functional>

namespace {
class NoteCard : public QFrame {
public:
    explicit NoteCard(const Annotation &annotation, QWidget *parent = nullptr)
        : QFrame(parent)
    {
        setObjectName("noteCard");
        setCursor(Qt::PointingHandCursor);
        setToolTip("点击跳转到这段划线");
        auto *layout = new QVBoxLayout(this);
        layout->setContentsMargins(11, 9, 11, 9);
        layout->setSpacing(5);

        auto *quote = new QLabel(this);
        quote->setObjectName("noteCardQuote");
        quote->setWordWrap(true);
        QString shown = annotation.quote.trimmed();
        if (shown.size() > 180) shown = shown.left(180) + QStringLiteral("…");
        quote->setText(QStringLiteral("“%1”").arg(shown));

        auto *note = new QLabel(this);
        note->setObjectName("noteCardNote");
        note->setWordWrap(true);
        note->setText(annotation.note.trimmed());
        note->setVisible(!annotation.note.trimmed().isEmpty());

        auto *time = new QLabel(this);
        time->setObjectName("noteCardTime");
        time->setText(QStringLiteral("记于 %1").arg(AnnotationPanel::timestampText(annotation.createdAt)));

        layout->addWidget(quote);
        layout->addWidget(note);
        layout->addWidget(time);
    }

    void setClicked(std::function<void()> callback) { m_clicked = std::move(callback); }

protected:
    void mouseReleaseEvent(QMouseEvent *event) override
    {
        QFrame::mouseReleaseEvent(event);
        if (event->button() == Qt::LeftButton && m_clicked) m_clicked();
    }

private:
    std::function<void()> m_clicked;
};
}

AnnotationPanel::AnnotationPanel(QWidget *parent) : QFrame(parent)
{
    setObjectName("noteSidebar");
    setFrameShape(QFrame::NoFrame);
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
    setMinimumWidth(0);

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(16, 16, 16, 14);
    root->setSpacing(9);

    auto *header = new QHBoxLayout;
    header->setSpacing(8);
    auto *title = new QLabel("读书笔记");
    title->setObjectName("notePanelTitle");
    m_countLabel = new QLabel("0 条");
    m_countLabel->setObjectName("noteTimestamp");
    auto *close = new QPushButton("×");
    close->setObjectName("noteCloseButton");
    close->setFixedSize(26, 26);
    close->setCursor(Qt::PointingHandCursor);
    close->setToolTip("收起笔记侧栏");
    header->addWidget(title);
    header->addWidget(m_countLabel);
    header->addStretch(1);
    header->addWidget(close);
    root->addLayout(header);

    auto *subtitle = new QLabel("划词记录 · 自动带时间戳");
    subtitle->setObjectName("notePanelSubtitle");
    root->addWidget(subtitle);
    root->addSpacing(2);

    auto *composer = new QFrame;
    composer->setObjectName("noteComposer");
    auto *composerLayout = new QVBoxLayout(composer);
    composerLayout->setContentsMargins(11, 10, 11, 10);
    composerLayout->setSpacing(7);

    auto *composerTitle = new QLabel("记录这一处");
    composerTitle->setObjectName("noteListHeader");
    m_quoteLabel = new QLabel;
    m_quoteLabel->setObjectName("noteQuote");
    m_quoteLabel->setWordWrap(true);
    m_hintLabel = new QLabel;
    m_hintLabel->setObjectName("noteComposerHint");
    m_hintLabel->setWordWrap(true);
    m_editor = new QPlainTextEdit;
    m_editor->setObjectName("noteEditor");
    m_editor->setPlaceholderText("写下此刻的想法…（可留空，只保留划线）");
    m_editor->setFixedHeight(78);
    m_editor->setTabChangesFocus(true);

    auto *composerFooter = new QHBoxLayout;
    composerFooter->setSpacing(8);
    m_timeLabel = new QLabel("记录时间：--");
    m_timeLabel->setObjectName("noteTimestamp");
    m_saveButton = new QPushButton("保存笔记");
    m_saveButton->setObjectName("primaryButton");
    m_saveButton->setEnabled(false);
    composerFooter->addWidget(m_timeLabel);
    composerFooter->addStretch(1);
    composerFooter->addWidget(m_saveButton);

    composerLayout->addWidget(composerTitle);
    composerLayout->addWidget(m_quoteLabel);
    composerLayout->addWidget(m_hintLabel);
    composerLayout->addWidget(m_editor);
    composerLayout->addLayout(composerFooter);
    root->addWidget(composer);

    auto *listHeader = new QLabel("全部笔记");
    listHeader->setObjectName("noteListHeader");
    root->addWidget(listHeader);

    m_scroll = new QScrollArea;
    m_scroll->setObjectName("noteScrollArea");
    m_scroll->setWidgetResizable(true);
    m_scroll->setFrameShape(QFrame::NoFrame);
    m_scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_noteContainer = new QWidget;
    m_noteContainer->setObjectName("noteCardArea");
    m_noteLayout = new QVBoxLayout(m_noteContainer);
    m_noteLayout->setContentsMargins(0, 0, 4, 0);
    m_noteLayout->setSpacing(8);
    m_scroll->setWidget(m_noteContainer);
    root->addWidget(m_scroll, 1);

    clearComposer();
    rebuildCards();

    connect(close, &QPushButton::clicked, this, &AnnotationPanel::closeRequested);
    connect(m_saveButton, &QPushButton::clicked, this, [this] {
        if (m_quote.isEmpty() || m_start < 0 || m_end <= m_start) return;
        emit saveRequested(m_start, m_end, m_quote, composerNote());
    });
}

void AnnotationPanel::setAnnotations(const QList<Annotation> &annotations)
{
    m_annotations = annotations;
    rebuildCards();
}

void AnnotationPanel::setSelection(int start, int end, const QString &quote)
{
    const QString trimmed = quote.trimmed();
    if (trimmed.isEmpty() || start < 0 || end <= start) {
        clearComposer();
        return;
    }
    m_start = start;
    m_end = end;
    m_quote = trimmed;
    QString preview = trimmed;
    preview.replace(QChar::ParagraphSeparator, ' ');
    preview.replace(QChar::LineSeparator, ' ');
    preview.replace('\n', ' ');
    if (preview.size() > 110) preview = preview.left(110) + QStringLiteral("…");
    m_quoteLabel->setText(QStringLiteral("“%1”").arg(preview));
    m_quoteLabel->setToolTip(trimmed);
    m_hintLabel->setText(QStringLiteral("已选择 %1 字 · 保存时自动记录当前时间").arg(trimmed.size()));
    m_timeLabel->setText(QStringLiteral("记录时间：%1")
                             .arg(QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm")));
    m_editor->setEnabled(true);
    m_saveButton->setEnabled(true);
    updateComposerState();
}

void AnnotationPanel::clearComposer()
{
    m_start = -1;
    m_end = -1;
    m_quote.clear();
    m_quoteLabel->setText("尚未选择文字");
    m_quoteLabel->setToolTip({});
    m_hintLabel->setText("在正文中划选一句后，这里会自动填入引用。");
    m_timeLabel->setText("记录时间：--");
    m_editor->clear();
    m_editor->setEnabled(false);
    m_saveButton->setEnabled(false);
}

void AnnotationPanel::focusComposer()
{
    if (m_start >= 0 && m_end > m_start) {
        m_editor->setFocus();
        m_editor->moveCursor(QTextCursor::End);
    }
}

QString AnnotationPanel::composerNote() const
{
    return m_editor->toPlainText().trimmed();
}

QStringList AnnotationPanel::displayedEntries() const
{
    QStringList entries;
    for (const Annotation &annotation : m_annotations) {
        QString entry = QStringLiteral("“%1”").arg(annotation.quote.trimmed());
        const QString note = annotation.note.trimmed();
        if (!note.isEmpty()) entry += QLatin1Char('\n') + note;
        entry += QStringLiteral("\n记于 %1").arg(timestampText(annotation.createdAt));
        entries.append(entry);
    }
    return entries;
}

QString AnnotationPanel::timestampText(const QDateTime &createdAt)
{
    if (!createdAt.isValid()) return QStringLiteral("时间未知");
    return createdAt.toLocalTime().toString("yyyy-MM-dd HH:mm");
}

void AnnotationPanel::rebuildCards()
{
    while (QLayoutItem *item = m_noteLayout->takeAt(0)) {
        if (QWidget *widget = item->widget()) widget->deleteLater();
        delete item;
    }
    if (m_annotations.isEmpty()) {
        auto *empty = new QLabel("还没有笔记。\n在正文里划选一句话，随时记下想法。");
        empty->setObjectName("noteEmpty");
        empty->setWordWrap(true);
        empty->setAlignment(Qt::AlignLeft | Qt::AlignTop);
        m_noteLayout->addWidget(empty);
    } else {
        for (const Annotation &annotation : m_annotations) {
            auto *card = new NoteCard(annotation, m_noteContainer);
            const int start = annotation.start;
            const int end = annotation.end;
            card->setClicked([this, start, end] { emit annotationActivated(start, end); });
            m_noteLayout->addWidget(card);
        }
    }
    m_noteLayout->addStretch(1);
    m_countLabel->setText(QStringLiteral("%1 条").arg(m_annotations.size()));
}

void AnnotationPanel::updateComposerState()
{
    if (m_start >= 0 && m_end > m_start) m_saveButton->setEnabled(true);
}
