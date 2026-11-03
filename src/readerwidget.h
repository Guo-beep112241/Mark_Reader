#pragma once

#include "book.h"

#include <QPixmap>
#include <QPlainTextEdit>
#include <QRect>

class QVariantAnimation;

// 阅读器正文控件。除了滚动阅读，还提供按页阅读和横向翻页动画；
// 中文字体与英文字体分别保存，英文由字符格式覆盖在中文字体之上。
class ReaderWidget : public QPlainTextEdit {
    Q_OBJECT
public:
    enum class PageMode {
        Scroll,
        Page,
    };

    explicit ReaderWidget(QWidget *parent = nullptr);

    int position() const;
    void setPosition(int position);

    void setPointSize(int pointSize);
    void setReaderFonts(const QString &chineseFamily, const QString &latinFamily, int pointSize);
    int pointSize() const { return m_pointSize; }
    QString chineseFamily() const { return m_chineseFamily; }
    QString latinFamily() const { return m_latinFamily; }

    bool findText(const QString &text);
    void selectRange(int start, int end);
    void showAnnotations(const QList<Annotation> &annotations, bool darkTheme);
    QString selectedText() const;
    int selectionStart() const;
    int selectionEnd() const;
    void setDarkTheme(bool enabled);
    bool darkTheme() const { return m_darkTheme; }

    void setPageMode(PageMode mode);
    PageMode pageMode() const { return m_pageMode; }
    int currentPage() const;
    int pageCount() const;
    void nextPage();
    void previousPage();
    // 测试与“减少动态效果”场景可以关闭翻页动画。
    void setAnimationsEnabled(bool enabled) { m_animationsEnabled = enabled; }
    bool animationsEnabled() const { return m_animationsEnabled; }

protected:
    void paintEvent(QPaintEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

signals:
    void positionChanged(int position);
    // 用户用鼠标划选文字后发出，附带视口坐标，方便在主窗口里放一个“记笔记”浮标。
    void selectionMade(const QString &text, int start, int end, const QRect &anchorRect);
    void selectionCleared();
    void pageInfoChanged(int currentPage, int totalPages);

private:
    void applyFonts();
    void applyLatinFont(const QString &latinFamily);
    void alignPageToPosition();
    void cancelPageTurn();
    void turnPage(int direction);
    void finishPageTurn();
    QPixmap captureViewport();
    void emitPageInfo();

    bool m_darkTheme = false;
    PageMode m_pageMode = PageMode::Scroll;
    QString m_chineseFamily;
    QString m_latinFamily;
    int m_pointSize = 18;
    bool m_animationsEnabled = true;
    bool m_turning = false;
    int m_flipDirection = 1;
    qreal m_flipProgress = 0.0;
    QPixmap m_flipFrom;
    QPixmap m_flipTo;
    int m_pendingPage = -1;
    QVariantAnimation *m_pageAnimation = nullptr;
    int m_wheelAccumulator = 0;
};
