#include "MapWidget.h"
#include "regions.h"
#include <QFile>
#include <QDomDocument>
#include <QDomElement>
#include <QPainterPath>
#include <QPen>
#include <QBrush>
#include <QTimer>
#include <QShowEvent>
#include <QRandomGenerator>
#include <QPainterPath>
#include <QRadialGradient>
#include <QDebug>
#include <cmath>

static const QColor kOceanBlue(28, 58, 94);   // 海洋蓝背景
static const QColor kLandBase(210, 210, 210); // 健康陆地基色
static const double kPI = 3.14159265358979323846;

MapWidget::MapWidget(QWidget *parent)
    : QGraphicsView(parent)
{
    m_scene = new QGraphicsScene(this);
    this->setScene(m_scene);

    this->setBackgroundBrush(QBrush(kOceanBlue)); // 海洋蓝
    this->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    this->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    this->setRenderHint(QPainter::Antialiasing);
    this->setTransformationAnchor(QGraphicsView::AnchorUnderMouse);

    m_regionItems.resize(Regions::count());
    m_regionCenters.fill(QPointF(), Regions::count());

    m_pinStart = makePin(0);
    m_pinRed = makePin(1);
    m_pinOrange = makePin(2);

    loadMap();
}

// 绘制“地图大头针”气泡：白色针 + 彩色圆头 + 图标（参照 graph/气泡）
QPixmap MapWidget::makePin(int type) const
{
    const qreal dpr = devicePixelRatioF() > 0 ? devicePixelRatioF() : 1.0;
    const int w = 34, h = 44;
    QPixmap pm(static_cast<int>(w * dpr), static_cast<int>(h * dpr));
    pm.setDevicePixelRatio(dpr);
    pm.fill(Qt::transparent);

    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    const QPointF c(w / 2.0, w / 2.0);
    const qreal R = w / 2.0 - 2;

    // 阴影
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(0, 0, 0, 70));
    p.drawEllipse(QPointF(w / 2.0, h - 4.0), 7.0, 3.0);

    // 白色大头针轮廓（圆头 + 下方三角）
    QPainterPath marker;
    marker.addEllipse(c, R + 2, R + 2);
    QPainterPath tail;
    tail.moveTo(w / 2.0, h - 2.0);
    tail.lineTo(c.x() - R * 0.7, c.y() + R * 0.55);
    tail.lineTo(c.x() + R * 0.7, c.y() + R * 0.55);
    tail.closeSubpath();
    marker = marker.united(tail);
    p.setBrush(Qt::white);
    p.drawPath(marker);

    // 彩色圆头
    QColor col = (type == 0) ? QColor(200, 22, 22)
               : (type == 1) ? QColor(150, 16, 42)
                             : QColor(232, 150, 30);
    QRadialGradient g(c - QPointF(R * 0.3, R * 0.3), R * 1.5);
    g.setColorAt(0.0, col.lighter(140));
    g.setColorAt(1.0, col);
    p.setBrush(g);
    p.setPen(Qt::NoPen);
    p.drawEllipse(c, R, R);

    // 图标
    if (type == 0) { // 开始：白色播放三角
        QPainterPath tri;
        tri.moveTo(c.x() - R * 0.32, c.y() - R * 0.5);
        tri.lineTo(c.x() + R * 0.55, c.y());
        tri.lineTo(c.x() - R * 0.32, c.y() + R * 0.5);
        tri.closeSubpath();
        p.setBrush(Qt::white);
        p.drawPath(tri);
    } else if (type == 1) { // 红色：生物危害风格三叶
        p.setBrush(Qt::white);
        for (int k = 0; k < 3; ++k) {
            double a = -kPI / 2 + k * 2 * kPI / 3;
            QPointF pc = c + QPointF(std::cos(a), std::sin(a)) * R * 0.45;
            p.drawEllipse(pc, R * 0.30, R * 0.30);
        }
        p.setBrush(col);
        p.drawEllipse(c, R * 0.28, R * 0.28);
        p.setBrush(Qt::white);
        p.drawEllipse(c, R * 0.12, R * 0.12);
    } else { // 橙色：DNA 双螺旋
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(QColor(45, 28, 8), 1.6));
        const double topY = c.y() - R * 0.6, botY = c.y() + R * 0.6, amp = R * 0.42;
        QPainterPath s1, s2;
        s1.moveTo(c.x() - amp, topY);
        s1.cubicTo(c.x() + amp, topY + (botY - topY) * 0.33,
                   c.x() - amp, topY + (botY - topY) * 0.66, c.x() + amp, botY);
        s2.moveTo(c.x() + amp, topY);
        s2.cubicTo(c.x() - amp, topY + (botY - topY) * 0.33,
                   c.x() + amp, topY + (botY - topY) * 0.66, c.x() - amp, botY);
        p.drawPath(s1);
        p.drawPath(s2);
        for (int k = 0; k < 3; ++k) {
            double yy = topY + (botY - topY) * (k + 0.5) / 3;
            p.drawLine(QPointF(c.x() - amp * 0.55, yy), QPointF(c.x() + amp * 0.55, yy));
        }
    }
    return pm;
}

void MapWidget::loadMap()
{
    QFile file(":/assets/worldChinaHigh.svg");
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qDebug() << "错误：无法打开地图资源文件！";
        return;
    }
    QDomDocument doc;
    if (!doc.setContent(&file)) {
        qDebug() << "错误：XML 解析失败！";
        file.close();
        return;
    }
    file.close();

    QDomNodeList pathList = doc.elementsByTagName("path");

    // 用于累计每个地区的中心点
    QVector<QPointF> centerSum(Regions::count(), QPointF(0, 0));
    QVector<int> centerCnt(Regions::count(), 0);

    for (int i = 0; i < pathList.count(); ++i) {
        QDomElement pathElement = pathList.at(i).toElement();
        if (pathElement.isNull())
            continue;

        QString countryName = pathElement.attribute("data-name");
        QString dStr = pathElement.attribute("d");
        if (countryName.isEmpty() || dStr.isEmpty())
            continue;

        QPainterPath path;
        int idx = 0;
        while (idx < dStr.length()) {
            QChar c = dStr[idx];
            if (c == 'M' || c == 'L') {
                char cmd = c.toLatin1();
                idx++;
                QString coordsStr;
                while (idx < dStr.length() && !dStr[idx].isLetter()) {
                    coordsStr += dStr[idx];
                    idx++;
                }
                QStringList points = coordsStr.split(',', Qt::SkipEmptyParts);
                if (points.size() >= 2) {
                    double px = points[0].toDouble();
                    double py = points[1].toDouble();
                    if (cmd == 'M')
                        path.moveTo(px, py);
                    else
                        path.lineTo(px, py);
                }
            } else if (c == 'Z' || c == 'z') {
                path.closeSubpath();
                idx++;
            } else {
                idx++;
            }
        }

        QGraphicsPathItem *item = new QGraphicsPathItem(path);
        item->setPen(QPen(QColor(40, 40, 40), 0.3));
        item->setBrush(QBrush(kLandBase));
        m_scene->addItem(item);

        m_countryItems[countryName] = item;

        int region = Regions::regionForCountry(countryName);
        if (region >= 0 && region < m_regionItems.size()) {
            m_regionItems[region].append(item);
            m_itemRegion[item] = region;
            QPointF center = item->sceneBoundingRect().center();
            centerSum[region] += center;
            centerCnt[region] += 1;
        }
        qDebug()<< countryName;
    }

    for (int r = 0; r < Regions::count(); ++r) {
        if (centerCnt[r] > 0)
            m_regionCenters[r] = centerSum[r] / centerCnt[r];
    }

    qDebug() << "地图加载完成，国家数：" << m_countryItems.size();
}

QColor MapWidget::colorForRegion(const RegionData &data) const
{
    double total = static_cast<double>(data.totalPopulation);
    if (total <= 0)
        return kLandBase;
    double inf = static_cast<double>(data.infectedCount) / total; // 感染占比
    double dead = static_cast<double>(data.deadCount) / total;    // 死亡占比
    inf = qBound(0.0, inf, 1.0);
    dead = qBound(0.0, dead, 1.0);

    // 感染越多越红，死亡越多越黑
    int base = 210;
    int r = static_cast<int>((base + (255 - base) * inf) * (1.0 - dead));
    int g = static_cast<int>(base * (1.0 - inf) * (1.0 - dead));
    int b = static_cast<int>(base * (1.0 - inf) * (1.0 - dead));
    return QColor(qBound(0, r, 255), qBound(0, g, 255), qBound(0, b, 255));
}

void MapWidget::onRegionUpdated(int regionId, RegionData data)
{
    if (regionId < 0 || regionId >= m_regionItems.size())
        return;
    QColor color = colorForRegion(data);
    for (QGraphicsPathItem *item : m_regionItems[regionId])
        item->setBrush(QBrush(color));
}

void MapWidget::spawnBubble(int regionId, bool isNewRegion)
{
    if (regionId < 0 || regionId >= m_regionCenters.size())
        return;
    QPointF c = m_regionCenters[regionId];
    if (c.isNull())
        return;

    // 红色 +3，橙色为 1~3 随机
    int value = isNewRegion ? 3 : (1 + QRandomGenerator::global()->bounded(3));
    const QPixmap &pm = isNewRegion ? m_pinRed : m_pinOrange;
    const qreal dpr = pm.devicePixelRatio() > 0 ? pm.devicePixelRatio() : 1.0;
    const qreal lw = pm.width() / dpr, lh = pm.height() / dpr;

    QGraphicsPixmapItem *bubble = m_scene->addPixmap(pm);
    bubble->setFlag(QGraphicsItem::ItemIgnoresTransformations, true);
    bubble->setOffset(-lw / 2.0, -lh); // 针尖对准地区中心
    bubble->setPos(c);
    bubble->setZValue(100);
    bubble->setToolTip(QString("点击收集 %1 DNA").arg(value));
    m_bubbleValue[bubble] = value;

    // 几秒后自动消失（未点击则错过）
    QGraphicsItem *ptr = bubble;
    QTimer::singleShot(6000, this, [this, ptr]() {
        if (m_bubbleValue.contains(ptr)) {
            m_bubbleValue.remove(ptr);
            m_scene->removeItem(ptr);
            delete ptr;
        }
    });
}

void MapWidget::showStartBubble(int regionId)
{
    clearStartBubble();
    if (regionId < 0 || regionId >= m_regionCenters.size())
        return;
    QPointF c = m_regionCenters[regionId];
    if (c.isNull())
        return;

    m_startRegion = regionId;
    const qreal dpr = m_pinStart.devicePixelRatio() > 0 ? m_pinStart.devicePixelRatio() : 1.0;
    const qreal lw = m_pinStart.width() / dpr, lh = m_pinStart.height() / dpr;
    m_startItem = m_scene->addPixmap(m_pinStart);
    m_startItem->setFlag(QGraphicsItem::ItemIgnoresTransformations, true);
    m_startItem->setOffset(-lw / 2.0, -lh);
    m_startItem->setPos(c);
    m_startItem->setZValue(200);
    m_startItem->setToolTip("点击此处开始：在该地区投放零号病人");
}

void MapWidget::clearStartBubble()
{
    if (m_startItem) {
        m_scene->removeItem(m_startItem);
        delete m_startItem;
        m_startItem = nullptr;
    }
    m_startRegion = -1;
}

void MapWidget::onGameStarted()
{
    m_seeded = true;
    clearStartBubble();
}

void MapWidget::onNewRegionInfected(int regionId, RegionData)
{
    spawnBubble(regionId, true);
}

void MapWidget::onInfectionSurge(int regionId, RegionData)
{
    spawnBubble(regionId, false);
}

void MapWidget::selectRegion(int regionId)
{
    // 清除旧高亮
    if (m_selectedRegion >= 0 && m_selectedRegion < m_regionItems.size()) {
        for (QGraphicsPathItem *item : m_regionItems[m_selectedRegion])
            item->setPen(QPen(QColor(40, 40, 40), 0.3));
    }
    m_selectedRegion = regionId;
    if (regionId >= 0 && regionId < m_regionItems.size()) {
        for (QGraphicsPathItem *item : m_regionItems[regionId])
            item->setPen(QPen(QColor(255, 235, 59), 1.4)); // 黄色高亮
    }
}

void MapWidget::mousePressEvent(QMouseEvent *event)
{
    QGraphicsItem *it = itemAt(event->pos());
    if (it) {
        // 点击开始气泡 -> 正式开始
        if (it == m_startItem && m_startRegion >= 0) {
            int region = m_startRegion;
            m_seeded = true;
            clearStartBubble();
            emit startRequested(region);
            return;
        }
        // DNA 气泡
        if (m_bubbleValue.contains(it)) {
            int amt = m_bubbleValue.take(it);
            m_scene->removeItem(it);
            delete it;
            emit dnaCollected(amt);
            return;
        }
        if (m_itemRegion.contains(it)) {
            int region = m_itemRegion.value(it);
            selectRegion(region);
            emit regionSelected(region);
            // 尚未开始：在所选地区显示“开始气泡”（可再次点击其它地区改选）
            if (!m_seeded)
                showStartBubble(region);
            return;
        }
    }
    // 点到海洋/空白
    selectRegion(-1);
    emit oceanClicked();
    QGraphicsView::mousePressEvent(event);
}

void MapWidget::wheelEvent(QWheelEvent *event)
{
    m_userZoomed = true; // 用户手动缩放后，不再自动适配
    if (event->angleDelta().y() > 0)
        scale(1.15, 1.15);
    else
        scale(0.87, 0.87);
}

void MapWidget::fitWholeWorld()
{
    if (m_userZoomed || m_scene->items().isEmpty())
        return;
    fitInView(m_scene->itemsBoundingRect(), Qt::KeepAspectRatio);
    // 视口尺寸在 show/resize 时可能还未最终确定，延迟一帧再适配一次以确保看到完整世界
    QTimer::singleShot(0, this, [this]() {
        if (!m_userZoomed && !m_scene->items().isEmpty())
            fitInView(m_scene->itemsBoundingRect(), Qt::KeepAspectRatio);
    });
}

void MapWidget::showEvent(QShowEvent *event)
{
    QGraphicsView::showEvent(event);
    fitWholeWorld();
}

void MapWidget::resizeEvent(QResizeEvent *event)
{
    QGraphicsView::resizeEvent(event);
    fitWholeWorld();
}
