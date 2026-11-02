#include "retrowidgets.h"

#include "theme.h"

#include <QFontMetrics>
#include <QLinearGradient>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QPolygonF>
#include <QStyleOptionViewItem>

PaperWidget::PaperWidget(QWidget *parent) : QWidget(parent)
{
    setObjectName(QStringLiteral("page"));
    setAttribute(Qt::WA_StyledBackground, false);
}

void PaperWidget::setDarkTheme(bool dark)
{
    if (m_dark == dark) return;
    m_dark = dark;
    update();
}

void PaperWidget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QPainter painter(this);
    Theme::paintBackdrop(painter, rect(), m_dark);
}

OrnamentRule::OrnamentRule(QWidget *parent) : QWidget(parent)
{
    setFixedHeight(16);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
}

void OrnamentRule::setDarkTheme(bool dark)
{
    if (m_dark == dark) return;
    m_dark = dark;
    update();
}

void OrnamentRule::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    const QColor gold = Theme::palette(m_dark).gold;
    const int centerY = height() / 2;
    const int centerX = width() / 2;
    QLinearGradient line(0, 0, width(), 0);
    line.setColorAt(0.0, QColor(gold.red(), gold.green(), gold.blue(), 0));
    line.setColorAt(0.18, QColor(gold.red(), gold.green(), gold.blue(), 140));
    line.setColorAt(0.82, QColor(gold.red(), gold.green(), gold.blue(), 140));
    line.setColorAt(1.0, QColor(gold.red(), gold.green(), gold.blue(), 0));
    painter.setPen(QPen(QBrush(line), 1));
    painter.drawLine(0, centerY, width(), centerY);

    for (int offset : {-1, 1}) {
        const QPoint center(centerX + offset * 26, centerY);
        painter.drawPolygon(QPolygonF({QPointF(center + QPoint(0, -3)), QPointF(center + QPoint(3, 0)),
                                       QPointF(center + QPoint(0, 3)), QPointF(center + QPoint(-3, 0))}));
    }
    painter.setBrush(QColor(gold.red(), gold.green(), gold.blue(), 190));
    painter.drawPolygon(QPolygonF({QPointF(centerX, centerY - 6), QPointF(centerX + 6, centerY),
                                   QPointF(centerX, centerY + 6), QPointF(centerX - 6, centerY)}));
}

CoverGridDelegate::CoverGridDelegate(QObject *parent) : QStyledItemDelegate(parent) {}

void CoverGridDelegate::setDarkTheme(bool dark) { m_dark = dark; }

QSize CoverGridDelegate::sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    Q_UNUSED(option);
    Q_UNUSED(index);
    return QSize(198, 320);
}

void CoverGridDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option,
                              const QModelIndex &index) const
{
    const Theme::Palette colors = Theme::palette(m_dark);
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);
    painter->setRenderHint(QPainter::SmoothPixmapTransform, true);

    const bool selected = option.state & QStyle::State_Selected;
    const bool hovered = option.state & QStyle::State_MouseOver;
    const QRect card = option.rect.adjusted(7, 6, -7, -10);

    painter->setPen(Qt::NoPen);
    QColor shadow = colors.shadow;
    painter->setBrush(shadow);
    painter->drawRoundedRect(card.translated(0, 4), 6, 6);

    QLinearGradient cardFill(card.topLeft(), card.bottomLeft());
    cardFill.setColorAt(0.0, selected ? colors.cardTop.lighter(104) : colors.cardTop);
    cardFill.setColorAt(1.0, selected ? colors.cardBottom.lighter(102) : colors.cardBottom);
    painter->setBrush(cardFill);
    QPen borderPen(selected ? colors.accent : (hovered ? colors.gold : colors.border), selected ? 2.0 : 1.0);
    painter->setPen(borderPen);
    painter->drawRoundedRect(card, 5, 5);

    QColor hairline = colors.gold;
    hairline.setAlpha(90);
    painter->setPen(QPen(hairline, 1));
    painter->setBrush(Qt::NoBrush);
    painter->drawRoundedRect(card.adjusted(4, 4, -4, -4), 3, 3);

    if (selected) {
        // 选中的书压一枚暗红色书签角，替代默认的蓝色选中条。
        QPainterPath ribbon;
        const QPoint topLeft(card.right() - 26, card.top() + 1);
        ribbon.moveTo(topLeft);
        ribbon.lineTo(topLeft + QPoint(25, 0));
        ribbon.lineTo(topLeft + QPoint(25, 30));
        ribbon.lineTo(topLeft + QPoint(12, 21));
        ribbon.lineTo(topLeft + QPoint(0, 30));
        ribbon.closeSubpath();
        painter->setPen(Qt::NoPen);
        painter->setBrush(colors.accent);
        painter->drawPath(ribbon);
    }

    const QSize coverSize(142, 213);
    const QRect coverRect(card.center().x() - coverSize.width() / 2, card.top() + 16,
                          coverSize.width(), coverSize.height());

    QColor coverShadow(0, 0, 0, m_dark ? 120 : 65);
    painter->setPen(Qt::NoPen);
    painter->setBrush(coverShadow);
    painter->drawRect(coverRect.translated(1, 4));

    const QPixmap cover = index.data(CoverRole).value<QPixmap>();
    if (!cover.isNull()) {
        const QPixmap scaled = cover.scaled(coverSize, Qt::KeepAspectRatioByExpanding,
                                            Qt::SmoothTransformation);
        const QRect source((scaled.width() - coverSize.width()) / 2,
                           (scaled.height() - coverSize.height()) / 2,
                           coverSize.width(), coverSize.height());
        painter->drawPixmap(coverRect, scaled, source);
    } else {
        QLinearGradient fallback(coverRect.topLeft(), coverRect.bottomLeft());
        fallback.setColorAt(0.0, colors.accent);
        fallback.setColorAt(1.0, colors.accentDark);
        painter->fillRect(coverRect, fallback);
        painter->setPen(colors.accentInk);
        painter->setFont(Theme::uiFont(34, true));
        painter->drawText(coverRect, Qt::AlignCenter, index.data(Qt::DisplayRole).toString().left(1));
    }

    QColor coverBorder = colors.gold;
    coverBorder.setAlpha(210);
    painter->setBrush(Qt::NoBrush);
    painter->setPen(QPen(coverBorder, 1));
    painter->drawRect(coverRect);
    painter->setPen(QPen(QColor(0, 0, 0, m_dark ? 120 : 60), 1));
    painter->drawRect(coverRect.adjusted(-1, -1, 1, 1));

    const int titleTop = coverRect.bottom() + 11;
    painter->setFont(Theme::uiFont(15, true));
    painter->setPen(colors.ink);
    const QString title = index.data(Qt::DisplayRole).toString();
    const QFontMetrics titleMetrics(painter->font());
    painter->drawText(QRect(card.left() + 8, titleTop, card.width() - 16, 22),
                      Qt::AlignHCenter | Qt::AlignVCenter,
                      titleMetrics.elidedText(title, Qt::ElideRight, card.width() - 16));

    painter->setFont(Theme::uiFont(11));
    painter->setPen(colors.inkMuted);
    const QString status = index.data(StatusRole).toString();
    painter->drawText(QRect(card.left() + 8, titleTop + 23, card.width() - 16, 18),
                      Qt::AlignHCenter | Qt::AlignVCenter, status);

    const int progress = index.data(ProgressRole).toInt();
    const QRect track(card.left() + 24, card.bottom() - 15, card.width() - 48, 3);
    QColor trackColor = colors.border;
    trackColor.setAlpha(120);
    painter->setPen(Qt::NoPen);
    painter->setBrush(trackColor);
    painter->drawRect(track);
    if (progress > 0) {
        const int filled = qBound(0, track.width() * progress / 100, track.width());
        painter->setBrush(colors.accent);
        painter->drawRect(QRect(track.left(), track.top(), filled, track.height()));
        painter->setBrush(colors.gold);
        painter->drawRect(QRect(track.left() + filled - 2, track.top() - 1, 3, track.height() + 2));
    }

    if (option.state & QStyle::State_HasFocus) {
        QColor focus = colors.gold;
        focus.setAlpha(180);
        painter->setBrush(Qt::NoBrush);
        painter->setPen(QPen(focus, 1, Qt::DotLine));
        painter->drawRoundedRect(card.adjusted(2, 2, -2, -2), 4, 4);
    }
    painter->restore();
}
