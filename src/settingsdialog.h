#pragma once

#include <QDialog>

class QComboBox;
class QLabel;
class QSpinBox;

// 阅读设置：分别选择中文字体和英文字体，并预览字号效果。
// 字体列表只包含本机已安装的候选，避免选中一个系统里不存在的名字。
class SettingsDialog : public QDialog {
    Q_OBJECT
public:
    explicit SettingsDialog(QWidget *parent = nullptr);

    void setChineseFont(const QString &family);
    void setLatinFont(const QString &family);
    void setPointSize(int pointSize);

    QString chineseFont() const;
    QString latinFont() const;
    int pointSize() const;

private:
    void updatePreview();

    QComboBox *m_chineseFont = nullptr;
    QComboBox *m_latinFont = nullptr;
    QSpinBox *m_pointSize = nullptr;
    QLabel *m_chinesePreview = nullptr;
    QLabel *m_latinPreview = nullptr;
};
