#ifndef MAPWIDGET_H
#define MAPWIDGET_H

#include <QGraphicsPathItem>
#include <QGraphicsPixmapItem>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QPixmap>
#include <QHash>
#include <QPointF>
#include <QRectF>
#include <QVector>
#include <QWheelEvent>
#include <QMouseEvent>
#include <QKeyEvent>
#include "GameTypes.h"

class QPainter;

class MapWidget : public QGraphicsView
{
    Q_OBJECT
public:
    explicit MapWidget(QWidget *parent = nullptr);

public slots:
    // 根据地区数据刷新该地区的感染/死亡密度（用红/黑点模拟）
    void onRegionUpdated(int regionId, RegionData data);
    // 新地区被感染：在该地区中心冒出红色 DNA 气泡
    void onNewRegionInfected(int regionId, RegionData data);
    // 感染激增：冒出橙色 DNA 气泡
    void onInfectionSurge(int regionId, RegionData data);
    // 游戏正式开始：清除开始气泡，之后点击地区不再放置开始气泡
    void onGameStarted();
    // 每过一天：递减 DNA 气泡寿命（受倍速/暂停影响）
    void onDayTick(int day);
    // 重开：清空气泡、点阵密度与选区
    void resetMap();

public:
    // 供点阵图层在 paint 时回调
    void paintDots(QPainter *painter);
    QRectF worldRect() const { return m_worldRect; }

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
    void buildRegionDots();                          // 预生成各地区点位
    QPixmap makePin(int type) const; // 0 开始 1 红色 2 橙色

    QGraphicsScene *m_scene;
    QHash<QString, QGraphicsPathItem *> m_countryItems; // 国家名 -> 图元
    QVector<QVector<QGraphicsPathItem *>> m_regionItems; // 地区 -> 其包含的国家图元
    QHash<QGraphicsItem *, int> m_itemRegion;            // 图元 -> 地区
    QVector<QPointF> m_regionCenters;                    // 地区中心（用于放气泡）
    QHash<QGraphicsItem *, int> m_bubbleValue;           // 气泡 -> DNA 数值
    QHash<QGraphicsItem *, int> m_bubbleLife;            // 气泡 -> 剩余天数
    int m_selectedRegion = -1;
    bool m_userZoomed = false;

    // 点阵模拟：每地区预生成点位 + 当前感染/死亡占比
    QVector<QVector<QPointF>> m_regionDots;
    QVector<double> m_regionInf;
    QVector<double> m_regionDead;
    double m_dotRadius = 1.0;
    QRectF m_worldRect;
    QGraphicsItem *m_dotsLayer = nullptr;
    QGraphicsPixmapItem *m_satItem = nullptr;            // 卫星底图

    // 卫星底图手动校准：可改顶部常量作为初始值，运行时也能用键盘实时微调
    QRectF m_satViewBox;                                 // 整幅范围（viewBox）
    double m_satPixW = 1.0, m_satPixH = 1.0;             // 卫星图像素尺寸
    double m_satOffX = 0.0, m_satOffY = 0.0;             // 平移（viewBox 单位，正=右/下）
    double m_satScaleX = 1.0, m_satScaleY = 1.0;         // 缩放微调（1=正好铺满）
    void applySatCalibration();                          // 依当前参数重新摆放卫星图

    QPixmap m_pinStart, m_pinRed, m_pinOrange;           // 三种气泡贴图
    QGraphicsPixmapItem *m_startItem = nullptr;          // 当前“开始气泡”
    int m_startRegion = -1;
    bool m_seeded = false;                               // 是否已正式开始

    void applyDefaultView(); // 进入地图时框定固定的局部视图（缩放+中心已定标）

    // 视图导航：鼠标拖拽平移 + 最小缩放限制（可键盘实时调参、标题显示）
    double m_minZoomFactor = 1.0;   // 最远缩放 = 铺满全图的倍数（1=刚好铺满）
    QRectF m_fitRect;               // 全图范围（计算“铺满缩放”用）
    bool m_panning = false;         // 左键是否按下中
    bool m_dragMoved = false;       // 本次按下是否已移动（拖拽 vs 点击）
    QPoint m_pressPos;              // 按下位置
    QPoint m_lastPanPos;            // 上次平移参考点
    double fitScale() const;                       // 铺满全图所需缩放
    void enforceMinZoom();                         // 当前缩放小于下限则拉回
    void handleClickAt(const QPoint &viewPos);     // 点击（非拖拽）时的选区/气泡处理

protected:
    void wheelEvent(QWheelEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void showEvent(QShowEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
};

#endif // MAPWIDGET_H
