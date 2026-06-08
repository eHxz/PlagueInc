#include "hudpanel.h"
#include "soundmanager.h"

#include <QPainter>
#include <QMouseEvent>

HudPanel::HudPanel(QWidget *parent)
    : QWidget(parent)
{
    setAttribute(Qt::WA_NoSystemBackground); // 透出地图
    setCursor(Qt::PointingHandCursor);
}

void HudPanel::setPixmap(const QPixmap &pm)
{
    m_pix = pm;
    update();
}

void HudPanel::setBgOpacity(qreal o)
{
    m_opacity = qBound(0.0, o, 1.0);
    update();
}

int HudPanel::addSlot(const QRectF &norm, Qt::Alignment align, const QColor &color,
                      qreal heightFrac, bool bold)
{
    Slot s;
    s.norm = norm;
    s.align = align;
    s.color = color;
    s.heightFrac = heightFrac;
    s.bold = bold;
    m_slots.append(s);
    return m_slots.size() - 1;
}

void HudPanel::setText(int slot, const QString &text)
{
    if (slot < 0 || slot >= m_slots.size())
        return;
    if (m_slots[slot].text == text)
        return;
    m_slots[slot].text = text;
    update();
}

QRectF HudPanel::drawnRect() const
{
    if (m_pix.isNull() || width() <= 0 || height() <= 0)
        return QRectF(rect());
    QSizeF s(m_pix.size());
    s.scale(width(), height(), Qt::KeepAspectRatio);
    const qreal x = (width() - s.width()) / 2.0;
    const qreal y = (height() - s.height()) / 2.0;
    return QRectF(x, y, s.width(), s.height());
}

void HudPanel::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::SmoothPixmapTransform);
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::TextAntialiasing);

    const QRectF dr = drawnRect();
    if (!m_pix.isNull()) {
        p.setOpacity(m_opacity);
        p.drawPixmap(dr, m_pix, QRectF(m_pix.rect()));
        p.setOpacity(1.0);
    }

    for (const Slot &s : m_slots) {
        if (s.text.isEmpty())
            continue;
        const QRectF r(dr.x() + s.norm.x() * dr.width(),
                       dr.y() + s.norm.y() * dr.height(),
                       s.norm.width() * dr.width(),
                       s.norm.height() * dr.height());
        QFont f = font();
        f.setBold(s.bold);
        f.setPixelSize(qMax(8, static_cast<int>(s.heightFrac * dr.height())));
        p.setFont(f);
        p.setPen(s.color);
        p.drawText(r, s.align | Qt::AlignVCenter, s.text);
    }
}

void HudPanel::mouseReleaseEvent(QMouseEvent *e)
{
    if (e->button() == Qt::LeftButton && rect().contains(e->pos())) {
        SoundManager::instance().playSfx(SoundManager::ButtonClick);
        emit clicked();
    }
    QWidget::mouseReleaseEvent(e);
}
