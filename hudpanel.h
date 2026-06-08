#ifndef HUDPANEL_H
#define HUDPANEL_H

#include <QWidget>
#include <QPixmap>
#include <QVector>
#include <QRectF>
#include <QColor>

// 主页面 HUD 图片面板：把一张（带透明度的）图按宽高比绘制，叠加若干文本槽，整体可点击。
// 用于左下角 DNA(disease.png)、右下角解药(cure.png)、下方正中世界条(world.png)、左上角新闻(news.png)。
class HudPanel : public QWidget
{
    Q_OBJECT
public:
    explicit HudPanel(QWidget *parent = nullptr);

    void setPixmap(const QPixmap &pm);
    QSize pixmapSize() const { return m_pix.size(); }
    void setBgOpacity(qreal o);

    // 添加文本槽：norm 为相对“绘制出的图片矩形”的归一化区域(0..1)，
    // heightFrac 为字号占绘制高度的比例。返回槽索引，用 setText 更新内容。
    int addSlot(const QRectF &norm, Qt::Alignment align, const QColor &color,
                qreal heightFrac, bool bold);
    void setText(int slot, const QString &text);

signals:
    void clicked();

protected:
    void paintEvent(QPaintEvent *) override;
    void mouseReleaseEvent(QMouseEvent *) override;

private:
    struct Slot {
        QRectF norm;
        Qt::Alignment align;
        QColor color;
        qreal heightFrac;
        bool bold;
        QString text;
    };
    QRectF drawnRect() const; // 按宽高比居中后的图片矩形

    QPixmap m_pix;
    qreal m_opacity = 0.85;
    QVector<Slot> m_slots;
};

#endif // HUDPANEL_H
