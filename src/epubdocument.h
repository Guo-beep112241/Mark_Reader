#pragma once

#include <QByteArray>
#include <QList>
#include <QString>

struct EpubChapter {
    QString title;
    QString text;
    int offset = 0;
};

struct EpubDocument {
    QString title;
    QString text;
    QList<EpubChapter> chapters;
    // EPUB 内嵌封面图的原始数据（通常是 JPEG/PNG）；没有封面时为空。
    QByteArray coverData;

    static bool load(const QString &path, EpubDocument *document, QString *error = nullptr);
};
