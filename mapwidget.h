#ifndef MAPWIDGET_H
#define MAPWIDGET_H

#include <QGraphicsPathItem>
#include <QGraphicsPixmapItem>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QPixmap>
#include <QHash>
#include <QSet>
#include <QPointF>
#include <QRectF>
#include <QVector>
#include <QWheelEvent>
#include <QMouseEvent>
#include <QKeyEvent>
#include "GameTypes.h"

class QPainter;
class QTimer;

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
    // 疾病属性变化：更新传染性（供海空载毒概率计算）
    void onDiseaseStatsChanged(DiseaseStats stats);
    // 某地区封锁变化：关闭机场/港口 -> 该类设施停发载具、拒收外来病例、图标变暗
    void onLockdownChanged(int regionId, bool airportClosed, bool portClosed, bool borderClosed);
    // 游戏速度变化：海空载具动画随之加速/暂停（暂停含进入菜单）
    void onGameSpeedChanged(int intervalMs);

public:
    // 供点阵图层在 paint 时回调
    void paintDots(QPainter *painter);
    QRectF worldRect() const { return m_worldRect; }

signals:
    void regionSelected(int regionId); // 点击某地区
    void oceanClicked();                // 点击海洋（空白）
    void dnaCollected(int amount);      // 点击气泡收集到 DNA
    void startRequested(int regionId);  // 点击“开始气泡” -> 投放零号病人
    void routeInfection(int regionId);  // 受感染的飞机/轮船抵达 -> 感染目标地区

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
    QVector<double> m_regionInf;       // 感染人数 / 总人口（红点密度可视化）
    QVector<double> m_regionInfAlive;  // 感染人数 / 存活人口（海空载毒概率，formula 形式二）
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

    // ——— 机场 / 港口设施 ———
    // 右键某国 -> 弹窗选择；机场落在点击处（陆地内部），港口吸附到最近海岸线（便于日后连航线）
    struct Facility {
        int type;                 // 0=机场 1=港口
        int region;               // 所属地区
        QPointF pos;              // 场景坐标（港口=海岸线上的点）
        QGraphicsPixmapItem *item;
        QGraphicsItem *cross = nullptr; // 封锁时显示的红叉（item 的子项，仅封锁可见）
    };
    QVector<Facility> m_facilities;
    QPixmap m_iconAirport, m_iconPort;                   // 两种设施图标（带白底徽章）
    QPixmap makeFacilityPin(int type) const;             // 0=机场 1=港口
    void handleRightClickAt(const QPoint &viewPos);      // 右键 -> 选择并放置设施
    void addFacility(int type, int region, const QPointF &scenePos, bool persist = true);
    // 命中：找出点所属地区（陆地内）或最近海岸的地区；返回该地区最近边界点与是否落在陆地内
    bool pickFacilityTarget(const QPointF &sp, int &region, QPointF &boundary, bool &inside) const;
    QPointF nearestBoundaryPoint(int region, const QPointF &sp) const; // 地区轮廓上离 sp 最近的点
    // 永久保存：设施写入/读取磁盘，对以后所有游戏固定
    QString facilitiesFilePath() const;
    void saveFacilities() const;
    void loadFacilities();
    // 导出设施编号表（序号 -> 类型 + 地区），作为日后航线权重图的节点ID
    void writeFacilityIndex() const;

    // ——— 海空交通：固定航线 + 动画载具 + 感染航线 ———
    // 一条航线的几何（按无序节点对缓存，正反向共用，保证路线固定不混乱）
    struct RouteGeom {
        QVector<QPointF> poly;   // 折线（“展开”坐标，可能超出图宽以表示绕东西边的太平洋航线）
        QVector<double>  cum;    // 累计弧长
        double           len = 0.0;
        bool             wrapped = false; // 是否跨越东西地图缝（需绘制平移副本）
        int              type = 0;        // 0=飞机直线 1=轮船海洋
    };
    struct RouteVis {
        QGraphicsPathItem *item = nullptr; // 红色虚线航线图元（仅被感染载具使用后出现）
        int hits = 0;                      // 被感染载具通过次数（越多越深）
    };
    struct Vehicle {
        int type = 0;        // 0=飞机 1=轮船
        int from = -1, to = -1;
        qint64 key = 0;      // 所属航线（无序对）键
        bool infected = false;
        bool forward = true; // 沿缓存折线的正向(低->高节点)还是反向
        double t = 0.0;      // 进度 0..1
        double dur = 1.0;    // 航行总时长（秒）
        QGraphicsPixmapItem *item = nullptr;
    };

    QHash<qint64, RouteGeom> m_routeGeom;   // 航线几何（启动时一次性预计算）
    QHash<qint64, RouteVis>  m_routeVis;    // 航线虚线显示状态
    QVector<Vehicle>         m_vehicles;    // 在途载具
    QPixmap m_icoPlane, m_icoShip, m_icoPlaneRed, m_icoShipRed;
    QTimer *m_transTimer = nullptr;         // 载具动画定时器（与游戏天数解耦，平滑移动）
    qint64  m_lastAnimMs = 0;
    double  m_infectivity = 0.0;            // 当前疾病传染性（来自 GameCore）
    double  m_routeModifier = 0.0;          // 海/空途径乘区（默认极低，可后续随技能提升）
    double  m_speedFactor = 1.0;            // 载具动画倍速（随游戏速度；暂停=0 冻结）
    QSet<int> m_airportClosed, m_portClosed; // 已封锁机场/港口的地区（来自 GameCore）
    void applyFacilityLockVisual(int region); // 按封锁状态调整该地区设施图标透明度
    double  m_maxPlaneLen = 1.0, m_maxShipLen = 1.0; // 归一化航程时长用

    // 海洋寻路网格（轮船航线 A* 用，启动时构建一次）
    QRectF        m_navRect;
    int           m_navCols = 0, m_navRows = 0;
    double        m_navCell = 0.0;
    QVector<uchar> m_navBlocked; // 1=禁航（陆地/北冰洋/底缘）
    QVector<int>   m_navClear;   // 距最近禁航格（陆/极）的格距：用于让航线尽量离岸
    QVector<int>   m_navComp;    // 连通水域编号（-1=禁航）：用于吸附时排除封闭小水塘
    QVector<int>   m_navCompSize;// 各连通水域的格数（按编号索引）

    // 海洋寻路辅助（场景坐标 -> 网格判断；带东西绕图）
    bool navBlockedAt(const QPointF &scene) const;  // 该点是否禁航（含越界=禁航）
    int  navClearAt(const QPointF &scene) const;    // 该点的离岸格距
    bool segNavigable(const QPointF &a, const QPointF &b, int minClear) const; // 直线段是否全程可航
    QPointF waterEdgeApproach(const QPointF &port, const QPointF &water) const; // 端点夹到离港最近海面点
    QVector<QPointF> smoothShipPath(const QVector<QPointF> &pts) const;         // 视线串拉简化 + 样条平滑

    QPixmap makeVehiclePixmap(const QString &res) const; // 白底去背 + 缩放（恒定大小）
    void initTransport();          // 载入载具贴图、建网格、预计算航线、起动画
    void buildNavGrid();           // 由国家形状栅格化出海/陆掩膜
    QVector<QPointF> computeShipPath(const QPointF &a, const QPointF &b) const; // A* 海洋折线
    void precomputeRoutes();       // 依 transportdata 预生成全部航线几何
    void buildRouteGeom(qint64 key, int type, const QVector<QPointF> &poly); // 填充 cum/len/wrapped
    void dispatchDepartures();     // 每个游戏日按平均发车间隔投放载具
    void spawnVehicle(int type, int from, int to, bool infected);
    void stepVehicles(double dtSec); // 动画推进
    void ensureRouteTrail(qint64 key); // 首次出现红色虚线
    void darkenTrail(qint64 key);      // 加深一档
    QPointF samplePath(const RouteGeom &g, double t, double &angleDeg) const; // 取位置+朝向(显示坐标)
    static qint64 pairKey(int a, int b) { return a < b ? (qint64)a * 100 + b : (qint64)b * 100 + a; }

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
