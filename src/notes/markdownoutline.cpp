#include "notes/markdownoutline.h"
#include <QRegularExpression>
#include <QTextDocument>

QVector<MarkdownHeading> markdownHeadings(const QString &text) {
    // 逐行维护围栏状态；只靠查找 # 会把代码示例误识别成文档标题。
    QVector<MarkdownHeading> result;
    const auto lines = text.split('\n');
    const QRegularExpression fence(R"(^ {0,3}(`{3,}|~{3,})(.*)$)");
    const QRegularExpression atx(R"(^ {0,3}(#{1,6})(?:\s+|$)(.*)$)");
    const QRegularExpression setext(R"(^ {0,3}(=+|-+)\s*$)");
    const QRegularExpression closing(R"(\s+#+\s*$)");
    QChar fenceChar; int fenceLength = 0; int previous = -1;
    auto title = [](const QString &raw) { QTextDocument document; document.setMarkdown(raw); return document.toPlainText().trimmed(); };
    for (int i = 0; i < lines.size(); ++i) {
        const auto &line = lines[i]; const auto matchFence = fence.match(line);
        // 关闭围栏必须字符一致且长度足够，尾部不能再带语言等额外内容。
        if (fenceLength) {
            if (matchFence.hasMatch() && matchFence.captured(1).front() == fenceChar && matchFence.captured(1).size() >= fenceLength && matchFence.captured(2).trimmed().isEmpty()) fenceLength = 0;
            previous = -1; continue;
        }
        if (matchFence.hasMatch()) { fenceChar = matchFence.captured(1).front(); fenceLength = matchFence.captured(1).size(); previous = -1; continue; }
        if (line.startsWith("    ") || line.startsWith('\t') || line.trimmed().isEmpty() || line.trimmed().startsWith('>')) { previous = -1; continue; }
        const auto heading = atx.match(line);
        if (heading.hasMatch()) {
            auto raw = heading.captured(2); raw.remove(closing);
            result.append({int(heading.captured(1).size()), title(raw), i}); previous = -1; continue;
        }
        const auto underline = setext.match(line);
        // Setext 标题由相邻的正文行与下划线组成，位置记录在正文行上。
        if (underline.hasMatch() && previous == i - 1) {
            result.append({underline.captured(1).front() == '=' ? 1 : 2, title(lines[previous]), previous}); previous = -1; continue;
        }
        previous = QRegularExpression(R"(^\s*(?:[-+*]\s|\d+[.)]\s))").match(line).hasMatch() ? -1 : i;
    }
    return result;
}
