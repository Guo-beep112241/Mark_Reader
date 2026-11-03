#pragma once

#include <QColor>
#include <QFont>
#include <QString>
#include <QStringList>

class QPainter;
class QPixmap;
class QRect;

// 复古阅读器的统一视觉语言：纸张色、墨色、烫金细节和字体选择都集中在这里，
// 书架、阅读页、按钮和封面绘制共用同一套颜色，避免各处风格走样。
namespace Theme {

struct Palette {
    bool dark = false;
    QColor backdropTop;
    QColor backdropBottom;
    QColor readerTop;
    QColor readerBottom;
    QColor cardTop;
    QColor cardBottom;
    QColor panelTop;
    QColor panelBottom;
    QColor field;
    QColor ink;
    QColor inkMuted;
    QColor accent;
    QColor accentDark;
    QColor accentInk;
    QColor gold;
    QColor border;
    QColor shadow;
    QColor selection;
    QColor highlight;
    QColor disabledInk;
    QColor disabledPanel;
};

Palette palette(bool dark);
QString styleSheet(bool dark);
QString readerStyleSheet(bool dark);
QStringList serifFamilies();
// 阅读字体自定义：只返回本机已安装的候选字体，避免下拉框里出现无法渲染的名字。
QStringList chineseFontChoices();
QStringList latinFontChoices();
QString defaultChineseFamily();
QString defaultLatinFamily();
QFont uiFont(int pixelSize, bool bold = false);
// 阅读器正文默认字体；需要中英文分别设置时由 ReaderWidget 按字符补上英文格式。
QFont readerFont(int pointSize);
QColor annotationColor(bool dark);

// 绘制做旧纸张背景：渐变纸色、细颗粒纹理、暗角与烫金双线边框。
void paintBackdrop(QPainter &painter, const QRect &rect, bool dark);
// 阅读页纸面：比书架更亮的羊皮纸，并保留很轻的边缘暗影。
void paintReaderPaper(QPainter &painter, const QRect &rect, bool dark);
QPixmap noiseTile(bool dark);

}
