#pragma once

#include "book.h"

#include <QDateTime>
#include <QFrame>

class QLabel;
class QPlainTextEdit;
class QPushButton;
class QScrollArea;
class QVBoxLayout;
class QWidget;

// 阅读页右侧的读书笔记侧栏：上面是划词引用和输入框，下面是带记录时间的笔记卡片。
class AnnotationPanel : public QFrame {
    Q_OBJECT
public:
    explicit AnnotationPanel(QWidget *parent = nullptr);

    void setAnnotations(const QList<Annotation> &annotations);
    QList<Annotation> annotations() const { return m_annotations; }
    int noteCount() const { return m_annotations.size(); }

    void setSelection(int start, int end, const QString &quote);
    void clearComposer();
    void focusComposer();
    int composerStart() const { return m_start; }
    int composerEnd() const { return m_end; }
    QString composerQuote() const { return m_quote; }
    QString composerNote() const;

    // 供测试和导出使用的纯文本摘要，格式与卡片上的显示一致。
    QStringList displayedEntries() const;
    static QString timestampText(const QDateTime &createdAt);

signals:
    void saveRequested(int start, int end, const QString &quote, const QString &note);
    void annotationActivated(int start, int end);
    void closeRequested();

private:
    void rebuildCards();
    void updateComposerState();

    QLabel *m_countLabel = nullptr;
    QLabel *m_quoteLabel = nullptr;
    QLabel *m_hintLabel = nullptr;
    QLabel *m_timeLabel = nullptr;
    QPlainTextEdit *m_editor = nullptr;
    QPushButton *m_saveButton = nullptr;
    QScrollArea *m_scroll = nullptr;
    QWidget *m_noteContainer = nullptr;
    QVBoxLayout *m_noteLayout = nullptr;
    QList<Annotation> m_annotations;
    int m_start = -1;
    int m_end = -1;
    QString m_quote;
};
