#include "theme.h"

#include <QFontDatabase>
#include <QImage>
#include <QLinearGradient>
#include <QPainter>
#include <QPixmap>
#include <QRadialGradient>
#include <QPolygonF>
#include <QRandomGenerator>

namespace Theme {

Palette palette(bool dark)
{
    Palette colors;
    colors.dark = dark;
    if (dark) {
        colors.backdropTop = QColor("#262119");
        colors.backdropBottom = QColor("#191510");
        colors.readerTop = QColor("#2a241b");
        colors.readerBottom = QColor("#201b14");
        colors.cardTop = QColor("#382f22");
        colors.cardBottom = QColor("#282217");
        colors.panelTop = QColor("#3a3226");
        colors.panelBottom = QColor("#2b251b");
        colors.field = QColor("#332b20");
        colors.ink = QColor("#e9dabd");
        colors.inkMuted = QColor("#b3a184");
        colors.accent = QColor("#8f3b2e");
        colors.accentDark = QColor("#6d2b21");
        colors.accentInk = QColor("#f4e6c8");
        colors.gold = QColor("#c3a469");
        colors.border = QColor("#5b4a33");
        colors.shadow = QColor(0, 0, 0, 110);
        colors.selection = QColor("#5c4a2f");
        colors.highlight = QColor("#5c4a24");
        colors.disabledInk = QColor("#6f6250");
        colors.disabledPanel = QColor("#2a241b");
    } else {
        colors.backdropTop = QColor("#ecdfc6");
        colors.backdropBottom = QColor("#ddc9a6");
        colors.readerTop = QColor("#f8f0dd");
        colors.readerBottom = QColor("#efe1c5");
        colors.cardTop = QColor("#f8efdb");
        colors.cardBottom = QColor("#e9d9b8");
        colors.panelTop = QColor("#f7edda");
        colors.panelBottom = QColor("#e4d3b1");
        colors.field = QColor("#faf2e2");
        colors.ink = QColor("#3a2e22");
        colors.inkMuted = QColor("#7c6a50");
        colors.accent = QColor("#7a2e26");
        colors.accentDark = QColor("#561c17");
        colors.accentInk = QColor("#f7e9cd");
        colors.gold = QColor("#a9854e");
        colors.border = QColor("#bda67e");
        colors.shadow = QColor(70, 45, 20, 60);
        colors.selection = QColor("#dcc69c");
        colors.highlight = QColor("#e8cf8f");
        colors.disabledInk = QColor("#a99880");
        colors.disabledPanel = QColor("#e6dcc8");
    }
    return colors;
}

QStringList serifFamilies()
{
    static const QStringList selected = [] {
        const QStringList candidates = {
            QStringLiteral("Songti SC"),
            QStringLiteral("STSong"),
            QStringLiteral("Kaiti SC"),
            QStringLiteral("Noto Serif CJK SC"),
            QStringLiteral("Source Han Serif SC"),
            QStringLiteral("Georgia"),
            QStringLiteral("Times New Roman"),
        };
        const QStringList installed = QFontDatabase::families();
        QStringList result;
        for (const QString &family : candidates) {
            if (installed.contains(family)) result.append(family);
        }
        if (result.isEmpty()) result.append(QStringLiteral("Georgia"));
        return result;
    }();
    return selected;
}

QStringList chineseFontChoices()
{
    static const QStringList selected = [] {
        const QStringList candidates = {
            QStringLiteral("Songti SC"),
            QStringLiteral("STSong"),
            QStringLiteral("Songti TC"),
            QStringLiteral("Kaiti SC"),
            QStringLiteral("STKaiti"),
            QStringLiteral("PingFang SC"),
            QStringLiteral("Heiti SC"),
            QStringLiteral("Hiragino Sans GB"),
            QStringLiteral("Hiragino Mincho ProN"),
            QStringLiteral("Noto Serif CJK SC"),
            QStringLiteral("Source Han Serif SC"),
        };
        const QStringList installed = QFontDatabase::families();
        QStringList result;
        for (const QString &family : candidates) {
            if (installed.contains(family) && !result.contains(family)) result.append(family);
        }
        return result;
    }();
    return selected;
}

QStringList latinFontChoices()
{
    static const QStringList selected = [] {
        const QStringList candidates = {
            QStringLiteral("Georgia"),
            QStringLiteral("Times New Roman"),
            QStringLiteral("Baskerville"),
            QStringLiteral("Palatino"),
            QStringLiteral("Charter"),
            QStringLiteral("Iowan Old Style"),
            QStringLiteral("Optima"),
            QStringLiteral("Avenir Next"),
            QStringLiteral("Garamond"),
        };
        const QStringList installed = QFontDatabase::families();
        QStringList result;
        for (const QString &family : candidates) {
            if (installed.contains(family) && !result.contains(family)) result.append(family);
        }
        return result;
    }();
    return selected;
}

QString defaultChineseFamily()
{
    const QStringList choices = chineseFontChoices();
    if (!choices.isEmpty()) return choices.first();
    return serifFamilies().value(0);
}

QString defaultLatinFamily()
{
    const QStringList choices = latinFontChoices();
    if (!choices.isEmpty()) return choices.first();
    return serifFamilies().value(0);
}

QFont uiFont(int pixelSize, bool bold)
{
    QFont font;
    font.setFamilies(serifFamilies());
    font.setPixelSize(pixelSize);
    font.setWeight(bold ? QFont::DemiBold : QFont::Normal);
    font.setStyleStrategy(QFont::PreferAntialias);
    return font;
}

QFont readerFont(int pointSize)
{
    QFont font;
    font.setFamilies(serifFamilies());
    font.setPointSize(pointSize);
    font.setStyleStrategy(QFont::PreferAntialias);
    return font;
}

QColor annotationColor(bool dark)
{
    return palette(dark).highlight;
}

QPixmap noiseTile(bool dark)
{
    // 使用进程级缓存避免每帧重建纹理。这里有意让 QPixmap 活到进程结束，
    // 不能放在静态对象里，否则 QGuiApplication 销毁后再析构会崩溃。
    static QPixmap *lightTile = nullptr;
    static QPixmap *darkTile = nullptr;
    QPixmap *&cached = dark ? darkTile : lightTile;
    if (cached) return *cached;

    constexpr int size = 190;
    QImage image(size, size, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing, false);

    QRandomGenerator random(dark ? 0x5eed51a7u : 0x51a77ea1u);
    for (int i = 0; i < 1500; ++i) {
        const int x = random.bounded(size);
        const int y = random.bounded(size);
        const int alpha = 3 + random.bounded(16);
        const QColor speck = random.bounded(2) == 0 ? QColor(255, 249, 232, alpha)
                                                    : QColor(dark ? 0 : 60, dark ? 0 : 42, dark ? 0 : 20, alpha);
        painter.setPen(speck);
        painter.drawPoint(x, y);
    }
    painter.setPen(QPen(QColor(dark ? 220 : 90, dark ? 205 : 65, dark ? 180 : 35, 12), 1));
    for (int i = 0; i < 70; ++i) {
        const int x = random.bounded(size);
        const int y = random.bounded(size);
        const int length = 4 + random.bounded(16);
        if (random.bounded(2) == 0)
            painter.drawLine(x, y, x + length, y + random.bounded(3) - 1);
        else
            painter.drawLine(x, y, x + random.bounded(3) - 1, y + length);
    }
    painter.end();
    cached = new QPixmap(QPixmap::fromImage(image));
    return *cached;
}

namespace {
void paintCornerOrnaments(QPainter &painter, const QRect &rect, const QColor &gold)
{
    QColor strong = gold;
    strong.setAlpha(150);
    painter.setBrush(Qt::NoBrush);
    painter.setPen(QPen(strong, 1));
    const int inset = 6;
    const int arm = 12;
    const QPoint corners[4] = {
        rect.topLeft() + QPoint(inset, inset),
        rect.topRight() + QPoint(-inset, inset),
        rect.bottomLeft() + QPoint(inset, -inset),
        rect.bottomRight() + QPoint(-inset, -inset),
    };
    const int dirs[4][2] = {{1, 1}, {-1, 1}, {1, -1}, {-1, -1}};
    for (int i = 0; i < 4; ++i) {
        const QPoint point = corners[i];
        const int dx = dirs[i][0];
        const int dy = dirs[i][1];
        painter.drawLine(point, point + QPoint(dx * arm, 0));
        painter.drawLine(point, point + QPoint(0, dy * arm));
        const QPoint diamond = point + QPoint(dx * 5, dy * 5);
        painter.drawPolygon(QPolygonF({QPointF(diamond + QPoint(0, -3)), QPointF(diamond + QPoint(3, 0)),
                                       QPointF(diamond + QPoint(0, 3)), QPointF(diamond + QPoint(-3, 0))}));
    }
}
}

void paintBackdrop(QPainter &painter, const QRect &rect, bool dark)
{
    const Palette colors = palette(dark);
    painter.save();
    painter.setRenderHint(QPainter::Antialiasing, true);

    QLinearGradient base(rect.topLeft(), rect.bottomLeft());
    base.setColorAt(0.0, colors.backdropTop);
    base.setColorAt(1.0, colors.backdropBottom);
    painter.fillRect(rect, base);
    painter.drawTiledPixmap(rect, noiseTile(dark));

    QRadialGradient light(rect.center() - QPoint(0, rect.height() / 4), qMax(rect.width(), rect.height()) * 0.62);
    light.setColorAt(0.0, QColor(255, 250, 235, dark ? 12 : 30));
    light.setColorAt(1.0, QColor(0, 0, 0, 0));
    painter.fillRect(rect, light);

    QRadialGradient vignette(rect.center(), qMax(rect.width(), rect.height()) * 0.74);
    vignette.setColorAt(0.52, QColor(0, 0, 0, 0));
    vignette.setColorAt(1.0, colors.shadow);
    painter.fillRect(rect, vignette);

    const QRect outer = rect.adjusted(12, 12, -13, -13);
    painter.setBrush(Qt::NoBrush);
    QColor gold = colors.gold;
    gold.setAlpha(140);
    painter.setPen(QPen(gold, 1));
    painter.drawRect(outer);
    gold.setAlpha(85);
    painter.setPen(QPen(gold, 1));
    painter.drawRect(outer.adjusted(4, 4, -4, -4));
    paintCornerOrnaments(painter, outer, colors.gold);
    painter.restore();
}

void paintReaderPaper(QPainter &painter, const QRect &rect, bool dark)
{
    const Palette colors = palette(dark);
    painter.save();
    painter.setRenderHint(QPainter::Antialiasing, false);

    QLinearGradient base(rect.topLeft(), rect.bottomLeft());
    base.setColorAt(0.0, colors.readerTop);
    base.setColorAt(1.0, colors.readerBottom);
    painter.fillRect(rect, base);
    painter.drawTiledPixmap(rect, noiseTile(dark));

    QRadialGradient light(rect.center() - QPoint(0, rect.height() / 5), qMax(rect.width(), rect.height()) * 0.7);
    light.setColorAt(0.0, QColor(255, 251, 238, dark ? 10 : 36));
    light.setColorAt(1.0, QColor(0, 0, 0, 0));
    painter.fillRect(rect, light);

    const int edge = qMin(22, rect.width() / 12);
    QLinearGradient left(rect.topLeft(), rect.topLeft() + QPoint(edge, 0));
    left.setColorAt(0.0, QColor(60, 40, 20, dark ? 70 : 34));
    left.setColorAt(1.0, QColor(0, 0, 0, 0));
    painter.fillRect(QRect(rect.left(), rect.top(), edge, rect.height()), left);
    QLinearGradient right(rect.topRight(), rect.topRight() - QPoint(edge, 0));
    right.setColorAt(0.0, QColor(60, 40, 20, dark ? 70 : 34));
    right.setColorAt(1.0, QColor(0, 0, 0, 0));
    painter.fillRect(QRect(rect.right() - edge, rect.top(), edge, rect.height()), right);
    QLinearGradient top(rect.topLeft(), rect.topLeft() + QPoint(0, 10));
    top.setColorAt(0.0, QColor(60, 40, 20, dark ? 46 : 22));
    top.setColorAt(1.0, QColor(0, 0, 0, 0));
    painter.fillRect(QRect(rect.left(), rect.top(), rect.width(), 10), top);
    painter.restore();
}

QString readerStyleSheet(bool dark)
{
    const Palette colors = palette(dark);
    return QStringLiteral(
               "QPlainTextEdit { background: transparent; color: @ink; border: 0;"
               " padding: 0; selection-background-color: @selection;"
               " selection-color: @ink; }")
        .replace("@ink", colors.ink.name())
        .replace("@selection", colors.selection.name());
}

QString styleSheet(bool dark)
{
    const Palette c = palette(dark);
    QString sheet = QStringLiteral(R"QSS(
QMainWindow { background: @backdrop; }
QMainWindow, QDialog { color: @ink; }
QDialog, QMessageBox, QInputDialog { background: @backdropTop; }
QStackedWidget, QWidget#page { background: transparent; }
QLabel { color: @ink; background: transparent; }
QLabel#shelfTitle { font-size: 31px; font-weight: 600; color: @ink; }
QLabel#shelfSubtitle { font-size: 11px; color: @gold; }
QLabel#bookTitle { font-size: 22px; font-weight: 600; color: @ink; }
QLabel#bookSubtitle { font-size: 10px; color: @gold; }
QLabel#toolLabel { font-size: 12px; color: @muted; }
QLabel#shelfCount, QLabel#readerFooter { font-size: 12px; color: @muted; }
QLabel#emptyState { font-size: 16px; color: @muted; }

QFrame#noteSidebar {
    background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 @panelTop, stop:1 @panelBottom);
    border: 1px solid @border;
    border-left: 2px solid @gold;
}
QLabel#notePanelTitle { font-size: 19px; font-weight: 600; color: @ink; }
QLabel#notePanelSubtitle { font-size: 10px; color: @gold; }
QLabel#noteListHeader { font-size: 13px; font-weight: 600; color: @ink; }
QLabel#noteQuote {
    color: @ink;
    background: @field;
    border-left: 2px solid @gold;
    padding: 6px 8px;
}
QLabel#noteComposerHint, QLabel#noteEmpty { font-size: 11px; color: @muted; }
QLabel#noteTimestamp { font-size: 11px; color: @gold; }
QFrame#noteComposer { background: transparent; border: 1px solid @border; border-radius: 3px; }
QPlainTextEdit#noteEditor {
    background: @field;
    color: @ink;
    border: 1px solid @border;
    border-radius: 3px;
    padding: 6px;
    selection-background-color: @selection;
    selection-color: @ink;
}
QPlainTextEdit#noteEditor:focus { border-color: @gold; }
QScrollArea#noteScrollArea { background: transparent; border: none; }
QScrollArea#noteScrollArea > QWidget > QWidget { background: transparent; }
QFrame#noteCard { background: @field; border: 1px solid @border; border-radius: 3px; }
QFrame#noteCard:hover { background: @panelTop; border-color: @gold; }
QLabel#noteCardQuote { color: @ink; font-size: 13px; }
QLabel#noteCardNote { color: @muted; font-size: 13px; }
QLabel#noteCardTime { color: @gold; font-size: 10px; }
QPushButton#noteCloseButton { padding: 1px 7px; min-height: 16px; font-size: 15px; }
QPushButton#selectionNoteButton {
    background: @accent;
    color: @accentInk;
    border: 1px solid @accentDark;
    border-bottom: 2px solid @gold;
    padding: 4px 10px;
    min-height: 16px;
    font-size: 12px;
}
QPushButton#selectionNoteButton:hover { background: @accentDark; border-color: @gold; }

QLabel#dialogTitle { font-size: 22px; font-weight: 600; color: @ink; }
QLabel#dialogSubtitle { font-size: 10px; color: @gold; }
QFrame#fontPreview { background: @field; border: 1px solid @border; border-left: 2px solid @gold; }
QLabel#fontPreviewChinese, QLabel#fontPreviewLatin { color: @ink; }
QSpinBox {
    background: @field;
    color: @ink;
    border: 1px solid @border;
    border-radius: 3px;
    padding: 6px 10px;
    min-height: 18px;
    font-size: 14px;
}
QSpinBox:focus { border-color: @gold; }

QFrame#readerFrame { background: transparent; border: 1px solid @border; }

QPushButton {
    background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 @panelTop, stop:1 @panelBottom);
    color: @ink;
    border: 1px solid @border;
    border-bottom: 2px solid @gold;
    border-radius: 3px;
    padding: 6px 14px;
    min-height: 18px;
    font-size: 14px;
}
QPushButton:hover {
    background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 @field, stop:1 @panelTop);
    border-color: @gold;
}
QPushButton:pressed {
    background: @panelBottom;
    border-top: 2px solid @gold;
    border-bottom: 1px solid @border;
    padding-top: 7px;
    padding-bottom: 5px;
}
QPushButton:disabled {
    background: @disabledPanel;
    color: @disabledInk;
    border-color: @border;
}
QPushButton:checked {
    background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 @accent, stop:1 @accentDark);
    color: @accentInk;
    border-color: @gold;
}
QPushButton#primaryButton {
    background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 @accent, stop:1 @accentDark);
    color: @accentInk;
    border: 1px solid @accentDark;
    border-bottom: 2px solid @accentDark;
    padding: 7px 18px;
    font-weight: 600;
}
QPushButton#primaryButton:hover {
    background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 @accent, stop:1 @accent);
    border-color: @gold;
}
QPushButton#primaryButton:pressed { background: @accentDark; }

QComboBox {
    background: @field;
    color: @ink;
    border: 1px solid @border;
    border-radius: 3px;
    padding: 6px 10px;
    min-height: 18px;
    font-size: 14px;
}
QComboBox:hover, QComboBox:focus { border-color: @gold; }
QComboBox::drop-down {
    subcontrol-origin: padding;
    subcontrol-position: center right;
    width: 24px;
    border-left: 1px solid @border;
}
QComboBox::down-arrow { image: url(:/theme/arrow-down.png); width: 12px; height: 8px; }
QComboBox QAbstractItemView {
    background: @field;
    color: @ink;
    border: 1px solid @border;
    selection-background-color: @accent;
    selection-color: @accentInk;
    padding: 4px;
    outline: 0;
}
QComboBox QAbstractItemView::item { padding: 5px 8px; min-height: 20px; }

QLineEdit {
    background: @field;
    color: @ink;
    border: 1px solid @border;
    border-radius: 3px;
    padding: 7px 10px;
    selection-background-color: @selection;
    selection-color: @ink;
    font-size: 14px;
}
QLineEdit:focus { border-color: @gold; }
QListWidget { background: transparent; border: none; outline: 0; }
QListWidget::item { background: transparent; border: none; }
QListWidget::item:selected { background: transparent; }

QScrollBar:vertical { background: transparent; width: 13px; margin: 2px; }
QScrollBar::handle:vertical {
    background: @border;
    min-height: 28px;
    border: 1px solid @gold;
    border-radius: 5px;
}
QScrollBar::handle:vertical:hover { background: @gold; }
QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; background: transparent; }
QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background: transparent; }
QScrollBar:horizontal { background: transparent; height: 13px; margin: 2px; }
QScrollBar::handle:horizontal {
    background: @border;
    min-width: 28px;
    border: 1px solid @gold;
    border-radius: 5px;
}
QScrollBar::handle:horizontal:hover { background: @gold; }
QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal { width: 0; background: transparent; }
QScrollBar::add-page:horizontal, QScrollBar::sub-page:horizontal { background: transparent; }

QStatusBar {
    background: @backdropBottom;
    color: @muted;
    border-top: 1px solid @border;
    font-size: 11px;
}
QStatusBar::item { border: none; }
QToolTip {
    background: @cardTop;
    color: @ink;
    border: 1px solid @border;
    padding: 4px 6px;
}
QMenu { background: @field; color: @ink; border: 1px solid @border; }
QMenu::item { padding: 5px 18px; }
QMenu::item:selected { background: @accent; color: @accentInk; }
)QSS");
    return sheet.replace("@backdropTop", c.backdropTop.name())
        .replace("@backdropBottom", c.backdropBottom.name())
        .replace("@backdrop", c.backdropTop.name())
        .replace("@panelTop", c.panelTop.name())
        .replace("@panelBottom", c.panelBottom.name())
        .replace("@field", c.field.name())
        .replace("@ink", c.ink.name())
        .replace("@muted", c.inkMuted.name())
        .replace("@accentInk", c.accentInk.name())
        .replace("@accentDark", c.accentDark.name())
        .replace("@accent", c.accent.name())
        .replace("@gold", c.gold.name())
        .replace("@border", c.border.name())
        .replace("@selection", c.selection.name())
        .replace("@disabledPanel", c.disabledPanel.name())
        .replace("@disabledInk", c.disabledInk.name())
        .replace("@cardTop", c.cardTop.name())
        .replace("@cardBottom", c.cardBottom.name());
}

}
