#pragma once
#include <QString>
#include <QVector>
struct MarkdownHeading { int level; QString title; int sourceLine; };
QVector<MarkdownHeading> markdownHeadings(const QString &text);
