#ifndef MAPWIDGET_H
#define MAPWIDGET_H

#include <QGraphicsPathItem>
#include <QGraphicsPixmapItem>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QPixmap>
#include <QHash>
#include <QPointF>
#include <QVector>
#include <QWheelEvent>
#include <QMouseEvent>
#include "GameTypes.h"

class MapWidget : public QGraphicsView
{
    Q_OBJECT
public:
    explicit MapWidget(QWidget *parent = nullptr);

public slots:
    // 根据地区数据刷新该地区所有国家的颜色（感染变红、死亡变黑）
    void onRegionUpdated(int regionId, RegionData data);
    // 新地区被感染：在该地区中心冒出红色 DNA 气泡
    void onNewRegionInfected(int regionId, RegionData data);
    // 感染激增：冒出橙色 DNA 气泡
    void onInfectionSurge(int regionId, RegionData data);
    // 游戏正式开始：清除开始气泡，之后点击地区不再放置开始气泡
    void onGameStarted();

signals:
    void regionSelected(int regionId); // 点击某地区
    void oceanClicked();                // 点击海洋（空白）
    void dnaCollected(int amount);      // 点击气泡收集到 DNA
    void startRequested(int regionId);  // 点击“开始气泡” -> 投放零号病人

private:
    void loadMap();
    void selectRegion(int regionId);
    void spawnBubble(int regionId, bool isNewRegion);
    void showStartBubble(int regionId);
    void clearStartBubble();
    QColor colorForRegion(const RegionData &data) const;
    QPixmap makePin(int type) const; // 0 开始 1 红色 2 橙色

    QGraphicsScene *m_scene;
    QHash<QString, QGraphicsPathItem *> m_countryItems; // 国家名 -> 图元
    QVector<QVector<QGraphicsPathItem *>> m_regionItems; // 地区 -> 其包含的国家图元
    QHash<QGraphicsItem *, int> m_itemRegion;            // 图元 -> 地区
    QVector<QPointF> m_regionCenters;                    // 地区中心（用于放气泡）
    QHash<QGraphicsItem *, int> m_bubbleValue;           // 气泡 -> DNA 数值
    int m_selectedRegion = -1;
    bool m_userZoomed = false;

    QPixmap m_pinStart, m_pinRed, m_pinOrange;           // 三种气泡贴图
    QGraphicsPixmapItem *m_startItem = nullptr;          // 当前“开始气泡”
    int m_startRegion = -1;
    bool m_seeded = false;                               // 是否已正式开始

    void fitWholeWorld();

protected:
    void wheelEvent(QWheelEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void showEvent(QShowEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
};

#endif // MAPWIDGET_H
