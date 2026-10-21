#pragma once
#include <QWidget>
#include <QIcon>
#include <QPixmap>
#include <QColor>
#include <QLabel>

// 使用绘图指令生成图标，不依赖外部图片文件或特定符号字体。
QIcon musicIcon(const QString &name, const QColor &color = QColor("#777982"));

// 空间不足时单行省略，避免长标题或保存状态撑大布局。
class ElidedLabel : public QLabel {
public:
    explicit ElidedLabel(const QString &text, QWidget *parent = nullptr);
    QSize minimumSizeHint() const override { return {0, QLabel::minimumSizeHint().height()}; }
    QSize sizeHint() const override { return {80, QLabel::sizeHint().height()}; }
protected:
    void paintEvent(QPaintEvent *) override;
};

// 显示可用封面；无封面时绘制项目原创的唱片占位图。
class CoverWidget : public QWidget {
public:
    explicit CoverWidget(QWidget *parent = nullptr);
    void setArtwork(const QPixmap &artwork);
protected:
    void paintEvent(QPaintEvent *) override;
private:
    QPixmap m_artwork;
};
