#include "settingsdialog.h"

#include "theme.h"

#include <QComboBox>
#include <QFormLayout>
#include <QFrame>
#include <QHBoxLayout>
#include <QHash>
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

namespace {
QString fontLabel(const QString &family)
{
    static const QHash<QString, QString> labels = {
        {QStringLiteral("Songti SC"), QStringLiteral("宋体（Songti SC）")},
        {QStringLiteral("STSong"), QStringLiteral("宋体（STSong）")},
        {QStringLiteral("Songti TC"), QStringLiteral("宋体（Songti TC，繁体）")},
        {QStringLiteral("Kaiti SC"), QStringLiteral("楷体（Kaiti SC）")},
        {QStringLiteral("STKaiti"), QStringLiteral("楷体（STKaiti）")},
        {QStringLiteral("PingFang SC"), QStringLiteral("苹方（PingFang SC）")},
        {QStringLiteral("Heiti SC"), QStringLiteral("黑体（Heiti SC）")},
        {QStringLiteral("Hiragino Sans GB"), QStringLiteral("冬青黑体（Hiragino Sans GB）")},
        {QStringLiteral("Hiragino Mincho ProN"), QStringLiteral("明朝（Hiragino Mincho ProN）")},
        {QStringLiteral("Noto Serif CJK SC"), QStringLiteral("思源宋体（Noto Serif CJK SC）")},
        {QStringLiteral("Source Han Serif SC"), QStringLiteral("思源宋体（Source Han Serif SC）")},
        {QStringLiteral("Georgia"), QStringLiteral("Georgia（衬线）")},
        {QStringLiteral("Times New Roman"), QStringLiteral("Times New Roman")},
        {QStringLiteral("Baskerville"), QStringLiteral("Baskerville（古典衬线）")},
        {QStringLiteral("Palatino"), QStringLiteral("Palatino（人文衬线）")},
        {QStringLiteral("Charter"), QStringLiteral("Charter（易读衬线）")},
        {QStringLiteral("Iowan Old Style"), QStringLiteral("Iowan Old Style（书卷衬线）")},
        {QStringLiteral("Optima"), QStringLiteral("Optima（人文无衬线）")},
        {QStringLiteral("Avenir Next"), QStringLiteral("Avenir Next（现代无衬线）")},
        {QStringLiteral("Garamond"), QStringLiteral("Garamond（古典衬线）")},
    };
    return labels.value(family, family);
}

void addFontChoices(QComboBox *combo, const QStringList &families, const QString &defaultLabel)
{
    combo->addItem(defaultLabel, QString());
    for (const QString &family : families)
        combo->addItem(fontLabel(family), family);
}
}

SettingsDialog::SettingsDialog(QWidget *parent) : QDialog(parent)
{
    setWindowTitle("阅读设置");
    setModal(true);
    resize(520, 500);

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(28, 24, 28, 22);
    root->setSpacing(10);

    auto *title = new QLabel("阅读设置");
    title->setObjectName("dialogTitle");
    auto *subtitle = new QLabel("MARK READER · 排版与字体");
    subtitle->setObjectName("dialogSubtitle");
    root->addWidget(title);
    root->addWidget(subtitle);
    root->addSpacing(6);

    auto *preview = new QFrame;
    preview->setObjectName("fontPreview");
    auto *previewLayout = new QVBoxLayout(preview);
    previewLayout->setContentsMargins(16, 14, 16, 14);
    previewLayout->setSpacing(6);
    m_chinesePreview = new QLabel("春江潮水连海平，海上明月共潮生。", preview);
    m_chinesePreview->setObjectName("fontPreviewChinese");
    m_chinesePreview->setWordWrap(true);
    m_latinPreview = new QLabel("The quick brown fox jumps over the lazy dog. 0123456789", preview);
    m_latinPreview->setObjectName("fontPreviewLatin");
    m_latinPreview->setWordWrap(true);
    previewLayout->addWidget(m_chinesePreview);
    previewLayout->addWidget(m_latinPreview);
    root->addWidget(preview);
    root->addSpacing(6);

    auto *form = new QFormLayout;
    form->setHorizontalSpacing(14);
    form->setVerticalSpacing(12);
    form->setLabelAlignment(Qt::AlignLeft | Qt::AlignVCenter);

    m_chineseFont = new QComboBox;
    m_chineseFont->setObjectName("chineseFontCombo");
    addFontChoices(m_chineseFont, Theme::chineseFontChoices(),
                   QStringLiteral("系统默认（%1）").arg(Theme::defaultChineseFamily()));
    m_latinFont = new QComboBox;
    m_latinFont->setObjectName("latinFontCombo");
    addFontChoices(m_latinFont, Theme::latinFontChoices(),
                   QStringLiteral("系统默认（%1）").arg(Theme::defaultLatinFamily()));
    m_pointSize = new QSpinBox;
    m_pointSize->setObjectName("fontSizeSpin");
    m_pointSize->setRange(12, 32);
    m_pointSize->setSuffix(" pt");
    m_pointSize->setValue(18);

    form->addRow("中文字体", m_chineseFont);
    form->addRow("英文字体", m_latinFont);
    form->addRow("字号", m_pointSize);
    root->addLayout(form);

    auto *hint = new QLabel("中英文可分别选择；列表来自本机已安装字体。部分字体可能缺少个别字形，"
                            "阅读器会自动回退到系统中文字体。");
    hint->setObjectName("noteComposerHint");
    hint->setWordWrap(true);
    root->addWidget(hint);
    root->addStretch(1);

    auto *buttons = new QHBoxLayout;
    buttons->addStretch(1);
    auto *cancel = new QPushButton("取消");
    auto *save = new QPushButton("保存");
    save->setObjectName("primaryButton");
    save->setDefault(true);
    buttons->addWidget(cancel);
    buttons->addWidget(save);
    root->addLayout(buttons);

    connect(cancel, &QPushButton::clicked, this, &QDialog::reject);
    connect(save, &QPushButton::clicked, this, &QDialog::accept);
    connect(m_chineseFont, qOverload<int>(&QComboBox::currentIndexChanged), this,
            [this] { updatePreview(); });
    connect(m_latinFont, qOverload<int>(&QComboBox::currentIndexChanged), this,
            [this] { updatePreview(); });
    connect(m_pointSize, qOverload<int>(&QSpinBox::valueChanged), this,
            [this] { updatePreview(); });
    updatePreview();
}

void SettingsDialog::setChineseFont(const QString &family)
{
    const int index = m_chineseFont->findData(family);
    m_chineseFont->setCurrentIndex(index > 0 ? index : 0);
    updatePreview();
}

void SettingsDialog::setLatinFont(const QString &family)
{
    const int index = m_latinFont->findData(family);
    m_latinFont->setCurrentIndex(index > 0 ? index : 0);
    updatePreview();
}

void SettingsDialog::setPointSize(int pointSize)
{
    m_pointSize->setValue(pointSize);
    updatePreview();
}

QString SettingsDialog::chineseFont() const
{
    return m_chineseFont->currentData().toString();
}

QString SettingsDialog::latinFont() const
{
    return m_latinFont->currentData().toString();
}

int SettingsDialog::pointSize() const
{
    return m_pointSize->value();
}

void SettingsDialog::updatePreview()
{
    QFont chinese(m_chineseFont->currentData().toString().isEmpty()
                      ? Theme::defaultChineseFamily()
                      : m_chineseFont->currentData().toString());
    chinese.setPointSize(m_pointSize->value());
    m_chinesePreview->setFont(chinese);

    QFont latin(m_latinFont->currentData().toString().isEmpty()
                    ? Theme::defaultLatinFamily()
                    : m_latinFont->currentData().toString());
    latin.setPointSize(m_pointSize->value());
    m_latinPreview->setFont(latin);
}
