#include "hexnode.h"
#include <QPainter>
#include <QPolygonF>
#include <QMouseEvent>
#include <cmath>

HexNode::HexNode(const QString &id, QWidget *parent)
    : QWidget(parent)
    , m_id(id)
{
    // 尺寸由父级蜂窝格子在 layoutHexes() 里按格大小设定（不再固定 64×64）
    setAttribute(Qt::WA_NoSystemBackground);    // 让六边形之外透出背景
    setCursor(Qt::PointingHandCursor);
}

void HexNode::setGlyph(const QString &g)
{
    m_glyph = g;
    update();
}

void HexNode::setPixmap(const QPixmap &pm)
{
    m_pix = pm;
    update();
}

void HexNode::setState(bool unlocked, bool affordable)
{
    m_unlocked = unlocked;
    m_affordable = affordable;
    update();
}

void HexNode::setSelected(bool s)
{
    m_selected = s;
    update();
}

void HexNode::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    const double w = width();
    const double h = height();
    const double cx = w / 2.0;
    const double cy = h / 2.0;
    const double pad = 4.0;
    // 平顶六边形（上下两条对边水平）：外接圆半径同时受 宽=2R、高=√3R 约束，
    // 取较小者保证六边形完整且居中，正好贴合所在的蜂窝格子。
    const double r = std::min((w - 2 * pad) / 2.0, (h - 2 * pad) / std::sqrt(3.0));

    QPolygonF hex;
    for (int i = 0; i < 6; ++i) {
        double ang = M_PI / 180.0 * (60.0 * i); // 顶点在 0°(右)，上下边水平 -> 平顶
        hex << QPointF(cx + r * std::cos(ang), cy + r * std::sin(ang));
    }

    if (!m_pix.isNull()) {
        // —— 用技能图标（六边形 PNG，已在外部旋转 90°）填充本格 ——
        // 等比缩放到六边形外接框（宽 2r、高 √3r）内，居中绘制。
        const double boxW = 2.0 * r;
        const double boxH = std::sqrt(3.0) * r;
        const double pw = m_pix.width(), ph = m_pix.height();
        const double sc = std::min(boxW / pw, boxH / ph);
        const double dw = pw * sc, dh = ph * sc;
        const QRectF target(cx - dw / 2.0, cy - dh / 2.0, dw, dh);

        // 状态：未解锁压暗（不可购买更暗），已解锁全亮。
        p.setOpacity(m_unlocked ? 1.0 : (m_affordable ? 0.9 : 0.45));
        p.drawPixmap(target, m_pix, QRectF(0, 0, pw, ph));
        p.setOpacity(1.0);

        // 已解锁：金色高亮描边；可购买：浅红描边提示。
        if (m_unlocked) {
            p.setBrush(Qt::NoBrush);
            p.setPen(QPen(QColor(255, 220, 120, 235), 2.4));
            p.drawPolygon(hex);
        } else if (m_affordable) {
            p.setBrush(Qt::NoBrush);
            p.setPen(QPen(QColor(255, 170, 175, 180), 1.6));
            p.drawPolygon(hex);
        }
    } else {
        // —— 回退：无图标时沿用纯色六边形 + 字形 ——
        QColor fill, border;
        if (m_unlocked) {
            fill = QColor(200, 36, 48);
            border = QColor(255, 210, 210);
        } else if (m_affordable) {
            fill = QColor(150, 28, 36);
            border = QColor(235, 150, 150);
        } else {
            fill = QColor(96, 26, 32, 230);
            border = QColor(150, 90, 95);
        }
        p.setBrush(fill);
        p.setPen(QPen(border, 1.5));
        p.drawPolygon(hex);

        QFont f = font();
        f.setPointSizeF(qMax(9.0, r * 0.62));
        p.setFont(f);
        p.setPen(m_unlocked ? QColor(255, 255, 255) : QColor(230, 210, 210));
        p.drawText(QRectF(0, 0, w, h), Qt::AlignCenter, m_glyph);
    }

    // 选中：黄色外环（覆盖于图标/纯色之上）
    if (m_selected) {
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(QColor(255, 235, 120), 2.6));
        QPolygonF outer;
        const double r2 = r + 2.0;
        for (int i = 0; i < 6; ++i) {
            const double ang = M_PI / 180.0 * (60.0 * i);
            outer << QPointF(cx + r2 * std::cos(ang), cy + r2 * std::sin(ang));
        }
        p.drawPolygon(outer);
    }
}

void HexNode::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton)
        emit clicked(m_id);
}
