#include "hexnode.h"
#include <QPainter>
#include <QPolygonF>
#include <QMouseEvent>
#include <cmath>

HexNode::HexNode(const QString &id, QWidget *parent)
    : QWidget(parent)
    , m_id(id)
{
    setFixedSize(64, 64);
    setAttribute(Qt::WA_NoSystemBackground);    // 让六边形之外透出背景
    setCursor(Qt::PointingHandCursor);
}

void HexNode::setGlyph(const QString &g)
{
    m_glyph = g;
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
    const double r = qMin(w, h) / 2.0 - 4.0;

    // 尖顶六边形
    QPolygonF hex;
    for (int i = 0; i < 6; ++i) {
        double ang = M_PI / 180.0 * (60.0 * i - 90.0);
        hex << QPointF(cx + r * std::cos(ang), cy + r * std::sin(ang));
    }

    QColor fill;
    QColor border;
    if (m_unlocked) {
        fill = QColor(200, 36, 48);   // 已解锁：亮红
        border = QColor(255, 210, 210);
    } else if (m_affordable) {
        fill = QColor(150, 28, 36);   // 可购买：中红
        border = QColor(235, 150, 150);
    } else {
        fill = QColor(96, 26, 32, 230); // 暂不可购买：暗红
        border = QColor(150, 90, 95);
    }

    p.setBrush(fill);
    p.setPen(QPen(border, m_selected ? 3.0 : 1.5));
    p.drawPolygon(hex);

    if (m_selected) {
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(QColor(255, 235, 120), 2.0));
        QPolygonF outer;
        double r2 = r + 3.0;
        for (int i = 0; i < 6; ++i) {
            double ang = M_PI / 180.0 * (60.0 * i - 90.0);
            outer << QPointF(cx + r2 * std::cos(ang), cy + r2 * std::sin(ang));
        }
        p.drawPolygon(outer);
    }

    // 图标
    QFont f = font();
    f.setPointSize(18);
    p.setFont(f);
    p.setPen(m_unlocked ? QColor(255, 255, 255) : QColor(230, 210, 210));
    p.drawText(QRectF(0, 0, w, h), Qt::AlignCenter, m_glyph);
}

void HexNode::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton)
        emit clicked(m_id);
}
