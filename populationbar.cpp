#include "populationbar.h"
#include <QPainter>
#include <QLocale>

PopulationBar::PopulationBar(QWidget *parent)
    : QWidget(parent)
{
    setMinimumHeight(16);
    setAttribute(Qt::WA_NoSystemBackground); // 透出下方地图
}

void PopulationBar::setData(const QString &title, long long healthy, long long infected, long long dead)
{
    m_title = title;
    m_healthy = qMax<long long>(0, healthy);
    m_infected = qMax<long long>(0, infected);
    m_dead = qMax<long long>(0, dead);
    update();
}

void PopulationBar::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    // 紧凑模式：高度较小（作为世界条下方的“感染进度条”）时只画彩色比例条本身，
    // 名称/数值已由上方的世界条展示，无需重复。
    if (height() < 34) {
        QRectF bar = rect().adjusted(6, 3, -6, -3);
        long long total = m_healthy + m_infected + m_dead;
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(20, 20, 20));
        p.drawRoundedRect(bar, 4, 4);
        if (total > 0) {
            double w = bar.width();
            double deadW = w * double(m_dead) / total;
            double infW = w * double(m_infected) / total;
            double x = bar.left();
            p.setBrush(QColor(10, 10, 10));
            p.drawRect(QRectF(x, bar.top(), deadW, bar.height())); x += deadW;
            p.setBrush(QColor(200, 30, 30));
            p.drawRect(QRectF(x, bar.top(), infW, bar.height())); x += infW;
            p.setBrush(QColor(40, 130, 210));
            p.drawRect(QRectF(x, bar.top(), w - deadW - infW, bar.height()));
        }
        p.setBrush(Qt::NoBrush);
        p.setPen(QColor(90, 90, 95));
        p.drawRoundedRect(bar, 4, 4);
        return;
    }

    const int margin = 12;
    const int titleH = 20;
    QRectF area = rect().adjusted(margin, 4, -margin, -8);

    // 标题：地区名 / 全球
    p.setPen(QColor(230, 230, 230));
    QFont f = font();
    f.setBold(true);
    f.setPointSize(11);
    p.setFont(f);
    p.drawText(QRectF(area.left(), area.top(), area.width(), titleH),
               Qt::AlignLeft | Qt::AlignVCenter, m_title);

    // 进度条区域
    QRectF bar(area.left(), area.top() + titleH, area.width(), area.height() - titleH);
    if (bar.height() < 8)
        bar.setHeight(8);

    long long total = m_healthy + m_infected + m_dead;
    // 背景
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(20, 20, 20));
    p.drawRoundedRect(bar, 4, 4);

    if (total > 0) {
        double w = bar.width();
        double deadW = w * static_cast<double>(m_dead) / total;
        double infW = w * static_cast<double>(m_infected) / total;
        double healthyW = w - deadW - infW;

        double x = bar.left();
        // 黑色：死亡
        p.setBrush(QColor(10, 10, 10));
        p.drawRect(QRectF(x, bar.top(), deadW, bar.height()));
        x += deadW;
        // 红色：感染
        p.setBrush(QColor(200, 30, 30));
        p.drawRect(QRectF(x, bar.top(), infW, bar.height()));
        x += infW;
        // 蓝色：健康
        p.setBrush(QColor(40, 130, 210));
        p.drawRect(QRectF(x, bar.top(), healthyW, bar.height()));
    }

    // 边框
    p.setBrush(Qt::NoBrush);
    p.setPen(QColor(80, 80, 80));
    p.drawRoundedRect(bar, 4, 4);

    // 数值文字（右侧）
    QLocale loc(QLocale::English);
    QString stat = QString("健康 %1   感染 %2   死亡 %3")
                       .arg(loc.toString(m_healthy))
                       .arg(loc.toString(m_infected))
                       .arg(loc.toString(m_dead));
    f.setBold(false);
    f.setPointSize(9);
    p.setFont(f);
    p.setPen(QColor(200, 200, 200));
    p.drawText(QRectF(area.left(), area.top(), area.width(), titleH),
               Qt::AlignRight | Qt::AlignVCenter, stat);
}
