#include "MapWidget.h"
#include <QDebug>

MapWidget::MapWidget(QWidget *parent) : QGraphicsView(parent) {
    m_scene = new QGraphicsScene(this);
    this->setScene(m_scene);

    // 设置背景色为深色，契合游戏风格
    this->setBackgroundBrush(Qt::black);

    loadMap();
}

void MapWidget::loadMap() {
    // TODO (B同学):
    // 1. 使用 QSvgRenderer 读取地图 SVG 文件
    // 2. 遍历路径，创建 QGraphicsPathItem 并加入 m_scene
    // 3. 将 Item 存入 m_countryItems 字典中，Key 是国家名
    qDebug() << "B同学正在加载地图...";
}

void MapWidget::onCountryDataUpdated(QString countryName, CountryData data) {
    // TODO (B同学):
    // 1. 计算感染比例 = data.infectedCount / data.totalPopulation
    // 2. 根据比例计算颜色（如：比例越高，红色越深）
    // 3. 从 m_countryItems 找到对应的 Item，调用 setBrush() 更新颜色
    qDebug() << "地图更新：" << countryName << " 感染人数：" << data.infectedCount;
}

void MapWidget::wheelEvent(QWheelEvent *event) {
    // TODO (B同学):
    // 实现滚轮缩放：if(event->angleDelta().y() > 0) scale(1.1, 1.1); else scale(0.9, 0.9);
}
