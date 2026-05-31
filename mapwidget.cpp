#include "MapWidget.h"
#include <QDebug>
#include <QFile>
#include <QDomDocument>
#include <QDomElement>
#include <QGraphicsPathItem>
#include <QWheelEvent>
#include <QPainterPath>
#include <QPen>
#include <QBrush>



MapWidget::MapWidget(QWidget *parent)
    : QGraphicsView(parent)
{
    m_scene = new QGraphicsScene(this);
    this->setScene(m_scene);

    // 设置背景色为深色，契合游戏风格
    this->setBackgroundBrush(Qt::black);

    this->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    this->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    this->setRenderHint(QPainter::Antialiasing);

    loadMap();
}

void MapWidget::loadMap()
{
    // TODO (B同学):
    // 1. 使用 QSvgRenderer 读取地图 SVG 文件
    // 2. 遍历路径，创建 QGraphicsPathItem 并加入 m_scene
    // 3. 将 Item 存入 m_countryItems 字典中，Key 是国家名
    qDebug() << "B同学正在加载地图...";
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

    for (int i = 0; i < pathList.count(); ++i) {
        QDomElement pathElement = pathList.at(i).toElement();
        if (pathElement.isNull()) continue;

        QString countryName = pathElement.attribute("data-name"); // 注释要求 Key 是国家名
        QString dStr = pathElement.attribute("d");               // 边界坐标数据

        if (countryName.isEmpty() || dStr.isEmpty()) continue;

        QPainterPath path;
        int idx = 0;
        while (idx < dStr.length()) {
            QChar c = dStr[idx];
            if (c == 'M' || c == 'L') {
                char cmd = c.toLatin1();
                idx++;

                // 提取坐标数字字符串（如 "957.93,483.21"）
                QString coordsStr;
                while (idx < dStr.length() && !dStr[idx].isLetter()) {
                    coordsStr += dStr[idx];
                    idx++;
                }

                QStringList points = coordsStr.split(',', Qt::SkipEmptyParts);
                if (points.size() >= 2) {
                    double px = points[0].toDouble();
                    double py = points[1].toDouble();
                    if (cmd == 'M') path.moveTo(px, py);
                    else if (cmd == 'L') path.lineTo(px, py);
                }
            } else if (c == 'Z' || c == 'z') {
                path.closeSubpath();
                idx++;
            } else {
                idx++;
            }
        }

        QGraphicsPathItem* item = new QGraphicsPathItem(path);

        item->setPen(QPen(QColor(40, 40, 40), 0.5));
        item->setBrush(QBrush(QColor(218, 218, 218)));

        m_scene->addItem(item);

        m_countryItems[countryName] = item;
    }

    qDebug() << "地图加载成功，共载入国家数量：" << m_countryItems.size();
}

void MapWidget::onCountryDataUpdated(QString countryName, CountryData data)
{
    // TODO (B同学):
    // 1. 计算感染比例 = data.infectedCount / data.totalPopulation
    // 2. 根据比例计算颜色（如：比例越高，红色越深）
    // 3. 从 m_countryItems 找到对应的 Item，调用 setBrush() 更新颜色
    qDebug() << "地图更新：" << countryName << " 感染人数：" << data.infectedCount;
    double ratio = 0.0;
    if (data.totalPopulation > 0) {
        ratio = static_cast<double>(data.infectedCount) / data.totalPopulation;
    }

    int r = 218 + static_cast<int>((255 - 218) * ratio);
    int g = static_cast<int>(218 * (1.0 - ratio));
    int b = static_cast<int>(218 * (1.0 - ratio));
    QColor virusColor(r, g, b);

    if (m_countryItems.contains(countryName)) {
        m_countryItems[countryName]->setBrush(QBrush(virusColor));
        qDebug() << "地图更新成功：" << countryName << " 实时感染率：" << (ratio * 100) << "%";
    } else {
        qDebug() << "警告：未在地图中找到国家：" << countryName;
    }
}

void MapWidget::wheelEvent(QWheelEvent *event)
{
    // TODO (B同学):
    // 实现滚轮缩放：if(event->angleDelta().y() > 0) scale(1.1, 1.1); else scale(0.9, 0.9);
    if (event->angleDelta().y() > 0) {
        scale(1.1, 1.1); // 向上滚，放大
    } else {
        scale(0.9, 0.9); // 向下滚，缩小
    }
}
