#pragma once

#include <QDateTime>
#include <QList>
#include <QString>
#include <QStringList>

struct Bookmark {
    QString id;
    int position = 0;
    QString label;
    QDateTime createdAt;
};

struct Annotation {
    QString id;
    int start = 0;
    int end = 0;
    QString quote;
    QString note;
    QDateTime createdAt;
};

struct Book {
    QString id;
    QString title;
    QString storedFileName;
    int position = 0;
    int characterCount = 0;
    QDateTime lastRead;
    QStringList chapterTitles;
    QList<int> chapterOffsets;
    QList<Bookmark> bookmarks;
    QList<Annotation> annotations;
};
