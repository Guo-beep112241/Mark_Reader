#pragma once

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

    static bool load(const QString &path, EpubDocument *document, QString *error = nullptr);
};
