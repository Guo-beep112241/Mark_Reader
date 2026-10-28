#pragma once

#include <QDateTime>
#include <QString>

struct Book {
    QString id;
    QString title;
    QString storedFileName;
    int position = 0;
    int characterCount = 0;
    QDateTime lastRead;
};
