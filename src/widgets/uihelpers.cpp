#include "widgets/uihelpers.h"
#include <QPainter>
#include <QPainterPath>
#include <QRadialGradient>

ElidedLabel::ElidedLabel(const QString &text, QWidget *parent) : QLabel(text, parent) {
    setTextFormat(Qt::PlainText); setToolTip(text); setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
}
void ElidedLabel::paintEvent(QPaintEvent *) {
    // 按当前可用宽度和字体度量绘制省略文本，原始 text 仍可供提示读取。
    QPainter painter(this); painter.setFont(font()); painter.setPen(palette().color(QPalette::WindowText));
    painter.drawText(contentsRect(), Qt::AlignLeft | Qt::AlignVCenter, fontMetrics().elidedText(text(), Qt::ElideRight, contentsRect().width()));
}

QIcon musicIcon(const QString &name, const QColor &color) {
    // 在两倍逻辑网格上绘制抗锯齿图标，统一形状、颜色和笔画风格。
    QPixmap pixmap(48, 48); pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.scale(2, 2);
    painter.setPen(QPen(color, 1.7, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    QPainterPath path;
    if (name == "play") { path.moveTo(8, 5); path.lineTo(19, 12); path.lineTo(8, 19); path.closeSubpath(); painter.fillPath(path, color); }
    else if (name == "pause") { painter.fillRect(QRectF(7, 5, 3, 14), color); painter.fillRect(QRectF(14, 5, 3, 14), color); }
    else if (name == "previous" || name == "next") {
        if (name == "previous") { painter.translate(24, 0); painter.scale(-1, 1); }
        path.moveTo(6, 6); path.lineTo(15, 12); path.lineTo(6, 18); path.closeSubpath(); painter.fillPath(path, color); painter.drawLine(18, 6, 18, 18);
    } else if (name == "heart" || name == "heartFilled") {
        path.moveTo(12, 20); path.cubicTo(9, 17, 3, 13, 3, 8); path.cubicTo(3, 3, 9, 2, 12, 7); path.cubicTo(15, 2, 21, 3, 21, 8); path.cubicTo(21, 13, 15, 17, 12, 20);
        if (name == "heartFilled") painter.fillPath(path, color); else painter.drawPath(path);
    } else if (name == "share") {
        painter.drawEllipse(QPointF(5, 12), 2.5, 2.5); painter.drawEllipse(QPointF(18, 5), 2.5, 2.5); painter.drawEllipse(QPointF(18, 19), 2.5, 2.5);
        painter.drawLine(QPointF(7.5, 10.5), QPointF(15.5, 6.5)); painter.drawLine(QPointF(7.5, 13.5), QPointF(15.5, 17.5));
    } else if (name == "volume") {
        path.moveTo(3, 9); path.lineTo(7, 9); path.lineTo(12, 5); path.lineTo(12, 19); path.lineTo(7, 15); path.lineTo(3, 15); path.closeSubpath(); painter.drawPath(path);
        painter.drawArc(QRectF(8, 6, 12, 12), -60 * 16, 120 * 16);
    } else if (name == "queue") {
        for (int i = 0; i < 3; ++i) { painter.drawPoint(4, 6 + i * 6); painter.drawLine(8, 6 + i * 6, 20, 6 + i * 6); }
    } else if (name == "repeat") {
        painter.drawArc(QRectF(4, 4, 16, 16), 30 * 16, 280 * 16); painter.drawLine(19, 5, 19, 10); painter.drawLine(19, 10, 14, 9);
        painter.drawText(QRectF(7, 5, 10, 14), Qt::AlignCenter, "1");
    } else if (name == "shuffle") {
        path.moveTo(3, 6); path.lineTo(7, 6); path.lineTo(16, 18); path.lineTo(21, 18); path.moveTo(3, 18); path.lineTo(7, 18); path.lineTo(16, 6); path.lineTo(21, 6); painter.drawPath(path);
        painter.drawLine(18, 3, 21, 6); painter.drawLine(18, 9, 21, 6); painter.drawLine(18, 15, 21, 18); painter.drawLine(18, 21, 21, 18);
    } else if (name == "left" || name == "right") {
        painter.drawRoundedRect(QRectF(3, 4, 18, 16), 2, 2); const int x = name == "left" ? 9 : 15; painter.drawLine(x, 4, x, 20);
    } else if (name == "focus") {
        path.moveTo(3, 8); path.lineTo(3, 3); path.lineTo(8, 3); path.moveTo(16, 3); path.lineTo(21, 3); path.lineTo(21, 8);
        path.moveTo(21, 16); path.lineTo(21, 21); path.lineTo(16, 21); path.moveTo(8, 21); path.lineTo(3, 21); path.lineTo(3, 16); painter.drawPath(path);
    } else if (name == "remove") {
        painter.drawLine(7, 7, 17, 17); painter.drawLine(17, 7, 7, 17);
    } else if (name == "import") {
        painter.drawLine(12, 3, 12, 15); painter.drawLine(8, 11, 12, 15); painter.drawLine(16, 11, 12, 15);
        path.moveTo(4, 15); path.lineTo(4, 20); path.lineTo(20, 20); path.lineTo(20, 15); painter.drawPath(path);
    } else {
        painter.drawLine(10, 4, 10, 17); painter.drawLine(10, 4, 19, 2); painter.drawLine(19, 2, 19, 15);
        painter.setBrush(color); painter.drawEllipse(QRectF(4, 14, 6, 5)); painter.drawEllipse(QRectF(13, 12, 6, 5));
    }
    return QIcon(pixmap);
}

CoverWidget::CoverWidget(QWidget *parent) : QWidget(parent) {
    setMinimumSize(120, 120); setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
}
void CoverWidget::setArtwork(const QPixmap &artwork) { m_artwork = artwork; update(); }
void CoverWidget::paintEvent(QPaintEvent *) {
    // 使用正方形圆角裁剪；有封面时居中裁切，无封面时绘制唱片占位图。
    QPainter painter(this); painter.setRenderHint(QPainter::Antialiasing);
    const qreal side = qMin(width(), height());
    const QRectF square((width() - side) / 2, (height() - side) / 2, side, side);
    QPainterPath clip; clip.addRoundedRect(square, 12, 12); painter.setClipPath(clip);
    if (!m_artwork.isNull()) {
        const auto scaled = m_artwork.scaled(square.size().toSize(), Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
        painter.drawPixmap(square.topLeft() + QPointF((side - scaled.width()) / 2, (side - scaled.height()) / 2), scaled);
        return;
    }
    QLinearGradient backdrop(square.topLeft(), square.bottomRight());
    backdrop.setColorAt(0, QColor("#f2d8cf")); backdrop.setColorAt(0.5, QColor("#e6a9a6")); backdrop.setColorAt(1, QColor("#b96976"));
    painter.fillRect(square, backdrop);
    painter.setPen(Qt::NoPen); painter.setBrush(QColor(255, 255, 255, 25));
    painter.drawEllipse(square.topLeft() + QPointF(side * .05, side * .10), side * .5, side * .5);
    const auto center = square.center();
    QRadialGradient record(center, side * .36); record.setColorAt(0, QColor("#45414a")); record.setColorAt(1, QColor("#26242e"));
    painter.setBrush(record); painter.drawEllipse(center, side * .35, side * .35);
    painter.setBrush(Qt::NoBrush); painter.setPen(QPen(QColor(255, 255, 255, 22), 1));
    for (int i = 0; i < 8; ++i) painter.drawEllipse(center, side * (.18 + i * .02), side * (.18 + i * .02));
    painter.setPen(Qt::NoPen); painter.setBrush(QColor("#f5ded6")); painter.drawEllipse(center, side * .12, side * .12);
    painter.setBrush(QColor("#bf5965")); painter.drawEllipse(center, side * .022, side * .022);
    painter.setPen(QColor(255, 255, 255, 220));
    QFont font = painter.font(); font.setPixelSize(qMax(9, int(side * .045))); font.setLetterSpacing(QFont::AbsoluteSpacing, 3); painter.setFont(font);
    if (side >= 120) painter.drawText(square.adjusted(0, 0, 0, -side * .05), Qt::AlignHCenter | Qt::AlignBottom, "T A L K M U S I C");
}
