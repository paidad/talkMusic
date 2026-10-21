#pragma once
#include <QString>
#include <QVector>
// sourceLine 从 0 开始；用位置区分同名标题，不能只按 title 定位。
struct MarkdownHeading { int level; QString title; int sourceLine; };
// 提取 ATX / Setext 标题并跳过代码围栏；不是完整 CommonMark 语法树解析器。
QVector<MarkdownHeading> markdownHeadings(const QString &text);
