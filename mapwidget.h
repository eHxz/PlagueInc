#ifndef MAPWIDGET_H
#define MAPWIDGET_H

#include <QGraphicsView>
#include <QGraphicsScene>
#include <QGraphicsPathItem>
#include <QWheelEvent>
#include <QMap>
#include "GameTypes.h"

class MapWidget : public QGraphicsView {
    Q_OBJECT
public:
    explicit MapWidget(QWidget *parent = nullptr);

public slots:
    // TODO: B同学实现这个函数，根据数据改变对应国家的颜色
    void onCountryDataUpdated(QString countryName, CountryData data);

private:
    // TODO: B同学实现这个函数，从SVG加载地图图元
    void loadMap();

    QGraphicsScene *m_scene;
    // 存储国家名字与地图图元的对应关系
    QMap<QString, QGraphicsPathItem*> m_countryItems;

protected:
    // TODO: B同学实现缩放和平移逻辑
    void wheelEvent(QWheelEvent *event) override;
};

#endif // MAPWIDGET_H
