#pragma once

#include <QImage>
#include <QSize>
#include <QString>

// 生成书籍封面。EPUB 内嵌封面优先由 EpubDocument 提供；TXT 或没有封面的
// EPUB 使用这里绘制的“布面精装”占位封面，保证书架上的每本书都有封面。
namespace CoverArt {

QImage renderFallback(const QString &title, const QString &caption = QString(),
                      const QSize &size = QSize(600, 900));
bool save(const QImage &image, const QString &path, QString *error = nullptr);

}
