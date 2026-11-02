#pragma once

#include <QStyledItemDelegate>
#include <QWidget>

// 书架和阅读页共用的做旧纸张背景控件。
class PaperWidget : public QWidget {
    Q_OBJECT
public:
    explicit PaperWidget(QWidget *parent = nullptr);
    void setDarkTheme(bool dark);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    bool m_dark = false;
};

// 金色细线加中心菱形的装饰分隔条，让工具栏与内容之间有古籍页的感觉。
class OrnamentRule : public QWidget {
    Q_OBJECT
public:
    explicit OrnamentRule(QWidget *parent = nullptr);
    void setDarkTheme(bool dark);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    bool m_dark = false;
};

// 书架卡片绘制：封面、书名、阅读进度和选中态都在这里统一呈现。
class CoverGridDelegate : public QStyledItemDelegate {
    Q_OBJECT
public:
    enum DataRole {
        StatusRole = Qt::UserRole + 1,
        ProgressRole = Qt::UserRole + 2,
        CoverRole = Qt::UserRole + 3,
    };

    explicit CoverGridDelegate(QObject *parent = nullptr);
    void setDarkTheme(bool dark);

    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override;
    void paint(QPainter *painter, const QStyleOptionViewItem &option,
               const QModelIndex &index) const override;

private:
    bool m_dark = false;
};
