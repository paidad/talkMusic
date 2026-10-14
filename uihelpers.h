#pragma once
#include <QWidget>
#include <QIcon>
#include <QPixmap>
#include <QColor>
#include <QLabel>

QIcon musicIcon(const QString &name, const QColor &color = QColor("#777982"));

class ElidedLabel : public QLabel {
public:
    explicit ElidedLabel(const QString &text, QWidget *parent = nullptr);
    QSize minimumSizeHint() const override { return {0, QLabel::minimumSizeHint().height()}; }
    QSize sizeHint() const override { return {80, QLabel::sizeHint().height()}; }
protected:
    void paintEvent(QPaintEvent *) override;
};

class CoverWidget : public QWidget {
public:
    explicit CoverWidget(QWidget *parent = nullptr);
    void setArtwork(const QPixmap &artwork);
protected:
    void paintEvent(QPaintEvent *) override;
private:
    QPixmap m_artwork;
};
