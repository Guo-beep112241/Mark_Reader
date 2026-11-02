#include "coverart.h"

#include "theme.h"

#include <QDir>
#include <QFileInfo>
#include <QFontMetrics>
#include <QLinearGradient>
#include <QPainter>
#include <QPolygonF>
#include <QRadialGradient>
#include <QRandomGenerator>

namespace {

struct CoverPalette {
    QColor top;
    QColor bottom;
};

quint32 stableHash(const QString &text)
{
    quint32 hash = 2166136261u;
    for (const QChar character : text) {
        hash ^= static_cast<quint32>(character.unicode());
        hash *= 16777619u;
    }
    return hash;
}

QColor mix(const QColor &first, const QColor &second, qreal amount)
{
    return QColor::fromRgbF(first.redF() + (second.redF() - first.redF()) * amount,
                            first.greenF() + (second.greenF() - first.greenF()) * amount,
                            first.blueF() + (second.blueF() - first.blueF()) * amount);
}

CoverPalette paletteFor(quint32 seed)
{
    static const QList<CoverPalette> palettes = {
        {QColor("#6b3a32"), QColor("#3c211d")},
        {QColor("#425244"), QColor("#263128")},
        {QColor("#3e4b5f"), QColor("#242d3b")},
        {QColor("#6d5733"), QColor("#40331e")},
        {QColor("#503956"), QColor("#2f2134")},
        {QColor("#3f5656"), QColor("#263838")},
        {QColor("#70452d"), QColor("#402518")},
    };
    return palettes.at(static_cast<int>(seed % static_cast<quint32>(palettes.size())));
}

QFont coverFont(int pixelSize, bool bold = false)
{
    QFont font;
    font.setFamilies(Theme::serifFamilies());
    font.setPixelSize(pixelSize);
    font.setWeight(bold ? QFont::DemiBold : QFont::Normal);
    font.setStyleStrategy(QFont::PreferAntialias);
    return font;
}

bool isCjk(const QChar &character)
{
    const ushort code = character.unicode();
    return (code >= 0x3400 && code <= 0x9fff) || (code >= 0xf900 && code <= 0xfaff)
        || (code >= 0x3040 && code <= 0x30ff) || (code >= 0xac00 && code <= 0xd7af);
}

bool prefersVertical(const QString &title)
{
    int cjk = 0;
    int visible = 0;
    for (const QChar character : title) {
        if (character.isSpace()) continue;
        ++visible;
        if (isCjk(character)) ++cjk;
    }
    return visible > 0 && cjk * 5 >= visible * 3 && title.size() <= 24;
}

void paintClothTexture(QPainter &painter, const QRect &rect, quint32 seed, bool dark)
{
    QRandomGenerator random(seed);
    const QColor lightSpeck = dark ? QColor(255, 240, 210, 14) : QColor(255, 245, 220, 20);
    const QColor darkSpeck = QColor(0, 0, 0, 18);
    for (int i = 0; i < 4200; ++i) {
        const int x = rect.left() + random.bounded(rect.width());
        const int y = rect.top() + random.bounded(rect.height());
        painter.setPen(random.bounded(2) == 0 ? lightSpeck : darkSpeck);
        painter.drawPoint(x, y);
    }
    painter.setPen(QPen(QColor(255, 240, 210, 7), 1));
    for (int y = rect.top(); y < rect.bottom(); y += 3) painter.drawLine(rect.left(), y, rect.right(), y);
}

void paintFrame(QPainter &painter, const QRect &rect, const QColor &gold)
{
    QColor bright = gold;
    bright.setAlpha(205);
    painter.setBrush(Qt::NoBrush);
    painter.setPen(QPen(bright, 2));
    painter.drawRect(rect.adjusted(26, 26, -26, -26));
    bright.setAlpha(120);
    painter.setPen(QPen(bright, 1));
    painter.drawRect(rect.adjusted(38, 38, -38, -38));

    painter.setPen(QPen(bright, 1));
    const QPoint corners[4] = {
        rect.topLeft() + QPoint(44, 44),
        rect.topRight() + QPoint(-44, 44),
        rect.bottomLeft() + QPoint(44, -44),
        rect.bottomRight() + QPoint(-44, -44),
    };
    const int dirs[4][2] = {{1, 1}, {-1, 1}, {1, -1}, {-1, -1}};
    for (int i = 0; i < 4; ++i) {
        const QPoint corner = corners[i];
        const int dx = dirs[i][0];
        const int dy = dirs[i][1];
        painter.drawLine(corner, corner + QPoint(dx * 34, 0));
        painter.drawLine(corner, corner + QPoint(0, dy * 34));
        const QPoint center = corner + QPoint(dx * 10, dy * 10);
        painter.drawPolygon(QPolygonF({QPointF(center + QPoint(0, -5)), QPointF(center + QPoint(5, 0)),
                                       QPointF(center + QPoint(0, 5)), QPointF(center + QPoint(-5, 0))}));
    }
}

void paintVerticalTitle(QPainter &painter, const QRect &area, const QString &title, const QColor &ink,
                        const QColor &shadow, int pixelSize)
{
    const QString visible = title.trimmed();
    QFont font = coverFont(pixelSize, false);
    painter.setFont(font);
    const int cell = qRound(pixelSize * 1.18);
    const int maxRows = qMax(3, area.height() / cell);
    const int columns = (visible.size() + maxRows - 1) / maxRows;
    const int gap = qMax(6, pixelSize / 9);
    const int blockWidth = columns * cell + (columns - 1) * gap;
    const int startX = area.center().x() - blockWidth / 2;
    const int rows = qMin(maxRows, visible.size());
    const int blockHeight = rows * cell;
    const int startY = area.center().y() - blockHeight / 2;

    int index = 0;
    for (int column = 0; column < columns; ++column) {
        const int x = startX + (columns - 1 - column) * (cell + gap);
        for (int row = 0; row < maxRows && index < visible.size(); ++row, ++index) {
            const QRect cellRect(x, startY + row * cell, cell, cell);
            painter.setPen(shadow);
            painter.drawText(cellRect.translated(2, 3), Qt::AlignCenter, visible.at(index));
            painter.setPen(ink);
            painter.drawText(cellRect, Qt::AlignCenter, visible.at(index));
        }
    }
}

void paintHorizontalTitle(QPainter &painter, const QRect &area, const QString &title, const QColor &ink,
                          const QColor &shadow, int pixelSize)
{
    QFont font = coverFont(pixelSize, true);
    painter.setFont(font);
    const QRect textArea = area.adjusted(8, 0, -8, 0);
    painter.setPen(shadow);
    painter.drawText(textArea.translated(2, 3), Qt::TextWordWrap | Qt::AlignCenter, title);
    painter.setPen(ink);
    painter.drawText(textArea, Qt::TextWordWrap | Qt::AlignCenter, title);
}

void paintSeal(QPainter &painter, const QPoint &topLeft, const QColor &gold)
{
    const QRect seal(topLeft, QSize(84, 84));
    QColor paper = gold;
    paper.setAlpha(225);
    painter.setPen(QPen(paper, 2));
    painter.setBrush(QColor("#8d2f26"));
    painter.drawRect(seal);
    QFont font = coverFont(50, true);
    painter.setFont(font);
    painter.setPen(QColor("#f2e3c4"));
    painter.drawText(seal, Qt::AlignCenter, QStringLiteral("藏"));
}

} // namespace

namespace CoverArt {

QImage renderFallback(const QString &title, const QString &caption, const QSize &size)
{
    const QSize canvas = size.isValid() && size.width() > 100 && size.height() > 100 ? size : QSize(600, 900);
    QImage image(canvas, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    const quint32 seed = stableHash(title);
    const CoverPalette colors = paletteFor(seed);
    const QColor gold("#c6a46a");
    const QColor ink("#f0e4c6");

    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);

    QLinearGradient background(0, 0, 0, canvas.height());
    background.setColorAt(0.0, colors.top);
    background.setColorAt(1.0, colors.bottom);
    painter.fillRect(image.rect(), background);
    paintClothTexture(painter, image.rect(), seed, true);

    QRadialGradient light(canvas.width() * 0.48, canvas.height() * 0.28, canvas.width() * 0.85);
    light.setColorAt(0.0, QColor(255, 240, 208, 34));
    light.setColorAt(1.0, QColor(0, 0, 0, 0));
    painter.fillRect(image.rect(), light);

    QRadialGradient vignette(QPointF(canvas.width() / 2.0, canvas.height() / 2.0), canvas.height() * 0.72);
    vignette.setColorAt(0.45, QColor(0, 0, 0, 0));
    vignette.setColorAt(1.0, QColor(0, 0, 0, 105));
    painter.fillRect(image.rect(), vignette);

    paintFrame(painter, image.rect(), gold);

    const QString displayTitle = title.trimmed().isEmpty() ? QStringLiteral("无名书") : title.trimmed();
    const QRect titleArea(70, 140, canvas.width() - 140, canvas.height() - 330);
    const QColor shadow(0, 0, 0, 120);
    if (prefersVertical(displayTitle)) {
        const int length = displayTitle.size();
        const int pixelSize = length <= 4 ? 112 : length <= 7 ? 94 : length <= 12 ? 76 : 60;
        paintVerticalTitle(painter, titleArea, displayTitle, ink, shadow, pixelSize);
    } else {
        const int length = displayTitle.size();
        const int pixelSize = length <= 10 ? 84 : length <= 18 ? 64 : length <= 30 ? 52 : 42;
        paintHorizontalTitle(painter, titleArea, displayTitle, ink, shadow, pixelSize);
    }

    paintSeal(painter, QPoint(70, canvas.height() - 214), gold);

    const int ruleWidth = canvas.width() - 180;
    QColor rule = gold;
    rule.setAlpha(170);
    painter.setPen(QPen(rule, 1));
    painter.drawLine(90, canvas.height() - 108, 90 + ruleWidth, canvas.height() - 108);
    QFont captionFont = coverFont(19, true);
    captionFont.setLetterSpacing(QFont::AbsoluteSpacing, 5);
    painter.setFont(captionFont);
    painter.setPen(rule);
    const QString finalCaption = caption.trimmed().isEmpty() ? QStringLiteral("MARK READER") : caption;
    painter.drawText(QRect(70, canvas.height() - 92, canvas.width() - 140, 40), Qt::AlignCenter,
                     finalCaption.toUpper());
    painter.end();
    return image;
}

bool save(const QImage &image, const QString &path, QString *error)
{
    if (image.isNull() || path.isEmpty()) {
        if (error) *error = QStringLiteral("封面图为空，无法保存。");
        return false;
    }
    QDir().mkpath(QFileInfo(path).absolutePath());
    if (!image.save(path, "PNG", 92)) {
        if (error) *error = QStringLiteral("无法保存封面：%1").arg(path);
        return false;
    }
    return true;
}

}
