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
#include <QStyleOptionGraphicsItem>
#include <QStringList>
#include <QTransform>
#include <QKeyEvent>
#include <QScrollBar>
#include <QMessageBox>
#include <QPushButton>
#include <QImage>
#include <QCoreApplication>
#include <QTextStream>
#include <QTimer>
#include <QElapsedTimer>
#include <QQueue>
#include <QPainter>
#include <QImage>
#include <QColor>
#include <QDebug>
#include <cmath>
#include <limits>
#include <queue>
#include <vector>
#include <algorithm>
#include <functional>
#include "gameparams.h"
#include "transportdata.h"
#include "soundmanager.h"

static const QColor kOceanBlue(8, 22, 40);     // 视口边缘留白色（卫星图之外）
static const double kPI = 3.14159265358979323846;
static const int kBubbleLifeDays = 5;          // DNA 气泡存活天数（随游戏时间流逝）

// 卫星图上的国家描边：淡白细线（政治边界感，不喧宾夺主）；选中时换为黄色
static const QPen kBorderPen(QColor(255, 255, 255, 70), 0.0);
static const QPen kHighlightPen(QColor(255, 235, 59), 1.4);

// ===================== 卫星底图手动校准参数 =====================
// 改完这 4 个数重新编译即可；也可在运行时用键盘实时微调后把标题栏显示的数值抄回来。
//   平移单位 = 地图坐标（整幅宽约 2752、高约 1538）。正值 = 向右 / 向下。
//   缩放 1.0 = 正好铺满整幅；>1 放大（拉伸），<1 缩小；X、Y 可分别调。
//   缩放以整幅中心为锚点（缩放时不跑偏，再用平移对位）。
// 运行时按键： 方向键=平移  Ctrl+方向键=单向拉伸  PgUp/PgDn=整体缩放  0=复位
static const double kSatOffsetX = 2.0;
static const double kSatOffsetY = 0.0;
static const double kSatScaleX  = 1.0980;
static const double kSatScaleY  = 1.0;
// ===============================================================

// 地图默认视图（已定标）：最小缩放系数 + 固定中心（归一化 0~1，落在整图的比例处）。
//   最小缩放 = 最远只能缩到“铺满全图”的此倍数（1.0=刚好铺满，>1=最远也更近，看到的是局部）。
static const double kMinZoomFactor = 1.10;
static const double kViewCenterNX  = 0.487; // 初始视图中心 X（占整图宽的比例）
static const double kViewCenterNY  = 0.492; // 初始视图中心 Y（占整图高的比例）

// 设施图标（机场/港口）：直接用原图（graph 里的黑底白标 JPG），不反转颜色、不加透明度。
// 贴图按较高分辨率烘焙，再以“场景尺寸”摆放（随地图缩放等比变化，而非恒定屏幕大小）。
static const double kFacilityTexPx     = 40.0;         // 设施贴图分辨率（像素；放大后仍清晰）
static const double kFacilitySceneSize = 16.0;         // 设施图标场景尺寸（场景单位，随地图缩放等比变化）

// 解析 SVG path 的 d 属性，支持 M/L/C/Z（含相对 m/l/c/z 与隐式重复命令）。
// 新版世界地图主要由绝对三次贝塞尔曲线 C 构成。
static QPainterPath parseSvgPath(const QString &d)
{
    QPainterPath path;
    const int n = d.length();
    int i = 0;
    QChar cmd;
    QPointF cur(0, 0), start(0, 0);

    auto skipSep = [&]() {
        while (i < n) {
            const QChar c = d[i];
            if (c == ' ' || c == ',' || c == '\t' || c == '\n' || c == '\r')
                ++i;
            else
                break;
        }
    };
    auto readNum = [&](bool &ok) -> double {
        skipSep();
        const int s = i;
        if (i < n && (d[i] == '+' || d[i] == '-'))
            ++i;
        while (i < n && (d[i].isDigit() || d[i] == '.'))
            ++i;
        if (i < n && (d[i] == 'e' || d[i] == 'E')) {
            ++i;
            if (i < n && (d[i] == '+' || d[i] == '-'))
                ++i;
            while (i < n && d[i].isDigit())
                ++i;
        }
        ok = (i > s);
        return ok ? d.mid(s, i - s).toDouble() : 0.0;
    };

    while (i < n) {
        skipSep();
        if (i >= n)
            break;
        if (d[i].isLetter()) {
            cmd = d[i];
            ++i;
        }
        const bool rel = cmd.isLower();
        const QChar C = cmd.toUpper();
        bool ok = true;
        if (C == 'M') {
            double x = readNum(ok), y = readNum(ok);
            if (!ok) break;
            if (rel) { x += cur.x(); y += cur.y(); }
            cur = QPointF(x, y);
            start = cur;
            path.moveTo(cur);
            cmd = rel ? QChar('l') : QChar('L'); // 后续隐式坐标作 lineto
        } else if (C == 'L') {
            double x = readNum(ok), y = readNum(ok);
            if (!ok) break;
            if (rel) { x += cur.x(); y += cur.y(); }
            cur = QPointF(x, y);
            path.lineTo(cur);
        } else if (C == 'C') {
            double x1 = readNum(ok), y1 = readNum(ok), x2 = readNum(ok),
                   y2 = readNum(ok), x = readNum(ok), y = readNum(ok);
            if (!ok) break;
            if (rel) {
                x1 += cur.x(); y1 += cur.y();
                x2 += cur.x(); y2 += cur.y();
                x  += cur.x(); y  += cur.y();
            }
            path.cubicTo(x1, y1, x2, y2, x, y);
            cur = QPointF(x, y);
        } else if (C == 'Z') {
            path.closeSubpath();
            cur = start;
        } else {
            // 未知命令：吞掉一个数字以保证推进，避免死循环
            readNum(ok);
            if (!ok) break;
        }
    }
    return path;
}

// 从 class 属性中取出两位 ISO 国家代码。class 形如 "land coast fr gf"（法属圭亚那）时
// 末尾才是具体辖区代码，父级 fr 在前，故取“最后一个”两位字母 token：
// "fr gf"->gf、"fr gp"->gp、"fr fx"->fx；"rs kosovo" 中 kosovo 非两位被跳过，仍取 rs。
static QString isoFromClass(const QString &cls)
{
    const QStringList toks = cls.split(' ', Qt::SkipEmptyParts);
    for (int k = toks.size() - 1; k >= 0; --k) {
        const QString &t = toks[k];
        if (t.size() == 2) {
            bool letters = true;
            for (QChar c : t)
                if (!c.isLetter()) { letters = false; break; }
            if (letters)
                return t.toLower();
        }
    }
    return QString();
}

// 点阵图层：位于国家填充之上、气泡之下，用红/黑点模拟感染与死亡密度
class DotsLayer : public QGraphicsItem
{
public:
    explicit DotsLayer(MapWidget *owner) : m_owner(owner)
    {
        setZValue(50);
        setAcceptedMouseButtons(Qt::NoButton); // 不拦截鼠标，点击穿透到下方国家
    }
    QRectF boundingRect() const override { return m_owner->worldRect(); }
    // 空形状：itemAt 命中检测会忽略本图层，点击落到下方国家上
    QPainterPath shape() const override { return QPainterPath(); }
    void paint(QPainter *p, const QStyleOptionGraphicsItem *, QWidget *) override
    {
        m_owner->paintDots(p);
    }

private:
    MapWidget *m_owner;
};

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
    m_minZoomFactor = kMinZoomFactor;

    m_regionItems.resize(Regions::count());
    m_regionCenters.fill(QPointF(), Regions::count());

    m_pinStart = makePin(0);
    // DNA 气泡改用新图标：红=Biohazard，橙=Nda（graph/气泡）；缺失则回退到程序绘制的针。
    {
        const int side = 44;
        QPixmap red(":/bubble/red.png");
        QPixmap org(":/bubble/orange.png");
        m_pinRed = red.isNull()
            ? makePin(1)
            : red.scaled(side, side, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        m_pinOrange = org.isNull()
            ? makePin(2)
            : org.scaled(side, side, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    }

    m_iconAirport = makeFacilityPin(0);
    m_iconPort = makeFacilityPin(1);

    loadMap();
    loadFacilities(); // 载入固定的机场/港口（对以后所有游戏生效）
    initTransport();  // 海空交通：贴图 / 海洋寻路网格 / 预计算固定航线 / 起动画
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

// 构建设施图标：直接用原图（黑底白标 JPG），仅按 0.25× 缩放，不反转颜色、不加透明度。
QPixmap MapWidget::makeFacilityPin(int type) const
{
    const QString res = (type == 0) ? ":/graph/airport.jpg" : ":/graph/port.jpg";
    QPixmap raw(res);
    if (raw.isNull())
        return QPixmap();

    // 烘焙为固定像素分辨率（dpr=1）；最终大小由 addFacility 按场景尺寸 setScale 决定，
    // 这样设施会随地图缩放等比变化、且放大后依然清晰。
    const int side = qMax(1, qRound(kFacilityTexPx));
    QPixmap scaled = raw.scaled(side, side, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    return scaled;
}

void MapWidget::loadMap()
{
    QFile file(":/assets/worldEquirect.svg");
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

    // 解析 viewBox，作为卫星底图与坐标系的整幅范围（等距圆柱：x=经度, y=纬度）
    QRectF viewBox(0, 0, 2752.766, 1537.631);
    {
        const QStringList vb = doc.documentElement().attribute("viewBox").split(' ', Qt::SkipEmptyParts);
        if (vb.size() == 4)
            viewBox = QRectF(vb[0].toDouble(), vb[1].toDouble(), vb[2].toDouble(), vb[3].toDouble());
    }

    QDomNodeList pathList = doc.elementsByTagName("path");

    // 国家代码可能不在 <path> 自身，而在祖先 <g class="land xx"> 上：多岛国家（澳大利亚、
    // 新西兰、日本、印尼、菲律宾、太平洋诸岛、法国海外属地…）用 <g> 包住若干无 class 的
    // <path>。逐路径向上找最近的 land class，否则这些国家会整块漏掉（无国界、不可点选）。
    auto landClassFor = [](QDomElement el) -> QString {
        for (QDomNode n = el; !n.isNull(); n = n.parentNode()) {
            const QDomElement e = n.toElement();
            if (e.isNull())
                continue;
            const QString cls = e.attribute("class");
            if (cls.split(' ', Qt::SkipEmptyParts).contains("land"))
                return cls;
        }
        return QString();
    };

    // 用于累计每个地区的中心点（按图元面积加权，使“开始气泡”落在主陆地而非小岛群）
    QVector<QPointF> centerSum(Regions::count(), QPointF(0, 0));
    QVector<double> centerW(Regions::count(), 0.0);

    for (int i = 0; i < pathList.count(); ++i) {
        QDomElement pathElement = pathList.at(i).toElement();
        if (pathElement.isNull())
            continue;

        const QString dStr = pathElement.attribute("d");
        if (dStr.isEmpty())
            continue;
        // 取承载国家代码的 class（自身或最近祖先）；非陆地（ocean/lake/经纬网）则为空，跳过
        const QString cls = landClassFor(pathElement);
        if (cls.isEmpty())
            continue;

        const QPainterPath path = parseSvgPath(dStr);
        if (path.isEmpty())
            continue;

        QGraphicsPathItem *item = new QGraphicsPathItem(path);
        item->setPen(kBorderPen);
        item->setBrush(Qt::NoBrush); // 透明：露出下方卫星底图，感染时由点阵层在其上着色
        // 缓存为位图：点阵层频繁重绘时无需反复栅格化复杂多边形，避免卡顿
        item->setCacheMode(QGraphicsItem::DeviceCoordinateCache);
        m_scene->addItem(item);

        const QString iso = isoFromClass(cls);
        m_countryItems[iso + '#' + QString::number(i)] = item;

        int region = Regions::regionForIso(iso);
        if (region >= 0 && region < m_regionItems.size()) {
            m_regionItems[region].append(item);
            m_itemRegion[item] = region;
            const QRectF bb = item->sceneBoundingRect();
            const double w = qMax(1.0, bb.width() * bb.height());
            centerSum[region] += bb.center() * w;
            centerW[region] += w;
        }
    }

    for (int r = 0; r < Regions::count(); ++r) {
        if (centerW[r] > 0.0)
            m_regionCenters[r] = centerSum[r] / centerW[r];
    }

    // 自检：列出没有任何图元的地区（应为空，否则说明该地区在 SVG 中没对应到陆地）
    QStringList missingRegions;
    for (int r = 0; r < m_regionItems.size(); ++r)
        if (m_regionItems[r].isEmpty())
            missingRegions << QString("%1 %2").arg(r).arg(Regions::names().value(r));
    if (missingRegions.isEmpty())
        qDebug() << "地区自检：全部" << Regions::count() << "个地区均已实现";
    else
        qDebug() << "地区自检：以下地区无图元 ->" << missingRegions;

    // 预生成各地区的点位，并加入点阵图层
    buildRegionDots();
    m_dotsLayer = new DotsLayer(this);
    m_scene->addItem(m_dotsLayer);

    // 卫星底图：缩放铺满整幅 viewBox（与等距圆柱国家坐标线性对齐），置于最底层
    QPixmap sat(":/assets/satellite.jpg");
    if (!sat.isNull()) {
        m_satItem = new QGraphicsPixmapItem(sat);
        m_satItem->setZValue(-10);
        m_satItem->setTransformationMode(Qt::SmoothTransformation);
        m_satItem->setAcceptedMouseButtons(Qt::NoButton);
        m_scene->addItem(m_satItem);
        // 记录尺寸与初始校准参数，再依参数摆放
        m_satViewBox = viewBox;
        m_satPixW = sat.width();
        m_satPixH = sat.height();
        m_satOffX = kSatOffsetX;
        m_satOffY = kSatOffsetY;
        m_satScaleX = kSatScaleX;
        m_satScaleY = kSatScaleY;
        applySatCalibration();
    }
    this->setRenderHint(QPainter::SmoothPixmapTransform, true); // 缩放卫星图时平滑采样

    // 全图范围（含卫星底图）：用于计算“铺满缩放”与最小缩放限制
    m_fitRect = m_scene->itemsBoundingRect();

    qDebug() << "地图加载完成，陆地图元数：" << m_countryItems.size();
}

// 在每个地区的国家形状内部采样一批点位（场景坐标），供点阵模拟使用
void MapWidget::buildRegionDots()
{
    m_worldRect = m_scene->itemsBoundingRect();
    m_regionDots.clear();
    m_regionDots.resize(Regions::count());
    m_regionInf.fill(0.0, Regions::count());
    m_regionInfAlive.fill(0.0, Regions::count());
    m_regionDead.fill(0.0, Regions::count());

    const double span = qMax(m_worldRect.width(), m_worldRect.height());
    double step = span / 320.0;          // 更密的采样网格（点更多）
    if (step <= 0.0)
        step = 2.0;
    m_dotRadius = step * 0.85;           // 点更大一些（配合透明度）

    const int cap = 500;                 // 每地区点数上限（按面积自然增长，封顶防过密）
    auto *rng = QRandomGenerator::global();

    for (int r = 0; r < m_regionItems.size(); ++r) {
        QVector<QPointF> pool;
        // 先收集整块地区内的全部候选点，覆盖南北全境（避免只落在北部）
        for (QGraphicsPathItem *item : m_regionItems[r]) {
            const QPainterPath path = item->path();
            const QRectF bb = path.boundingRect();
            for (double y = bb.top(); y <= bb.bottom(); y += step) {
                for (double x = bb.left(); x <= bb.right(); x += step) {
                    QPointF pt(x + (rng->generateDouble() - 0.5) * step,
                               y + (rng->generateDouble() - 0.5) * step);
                    if (path.contains(pt))
                        pool.append(pt);
                }
            }
        }
        // 打乱次序：使点的“出现”是全境散布而非逐行；也便于封顶时均匀抽样
        for (int i = pool.size() - 1; i > 0; --i)
            pool.swapItemsAt(i, rng->bounded(i + 1));
        // 面积过大时随机抽样封顶（在打乱之后截断 -> 仍是空间均匀的子集）
        if (pool.size() > cap)
            pool.resize(cap);
        m_regionDots[r] = pool;
    }
}

// 由点阵图层回调：
//  - 早期（覆盖率低）用红/黑点模拟密度；
//  - 某地区点足够密集（超过阈值）后改为整块填充该地区国家形状，不再逐点绘制（省去后期上千次描点，避免卡顿），
//    填充色的深浅/透明度随感染、死亡比例继续变化，最终整块趋于纯色（感染红 -> 死亡黑）。
void MapWidget::paintDots(QPainter *p)
{
    const QColor red(200, 30, 30), black(15, 15, 18);
    const double fillT = 0.6; // 覆盖率阈值：超过即改为整块填充

    for (int r = 0; r < m_regionDots.size(); ++r) {
        const double inf = (r < m_regionInf.size()) ? m_regionInf[r] : 0.0;
        const double dead = (r < m_regionDead.size()) ? m_regionDead[r] : 0.0;
        const double coverage = qMin(1.0, inf + dead);
        if (coverage <= 0.0)
            continue;

        // —— 密集阶段：整块填充 ——
        if (coverage > fillT && r < m_regionItems.size()) {
            // 透明度随覆盖率从阈值处的 185 平滑升到 255（最终近乎纯色）
            const double t = (coverage - fillT) / (1.0 - fillT);
            const int alpha = qBound(0, static_cast<int>(std::lround(185.0 + t * 70.0)), 255);
            // 颜色随死亡比例由红渐变到黑
            const double deadShare = (inf + dead) > 0.0 ? dead / (inf + dead) : 0.0;
            QColor c(static_cast<int>(red.red() * (1.0 - deadShare) + black.red() * deadShare),
                     static_cast<int>(red.green() * (1.0 - deadShare) + black.green() * deadShare),
                     static_cast<int>(red.blue() * (1.0 - deadShare) + black.blue() * deadShare));
            c.setAlpha(alpha);
            // 整块填充后再描白色国界：填充层在国家图元之上，纯色会盖住下方国界线，
            // 故每块填充后补描一次 kBorderPen，保证“白线完整”。
            for (QGraphicsPathItem *item : m_regionItems[r]) {
                p->fillPath(item->path(), c);
                p->strokePath(item->path(), kBorderPen);
            }
            continue;
        }

        // —— 早期阶段：红/黑点模拟密度 ——
        const QVector<QPointF> &pool = m_regionDots[r];
        const int n = pool.size();
        if (n == 0)
            continue;
        const QColor rdot(200, 30, 30, 185), bdot(15, 15, 18, 205);
        const int blackN = qBound(0, static_cast<int>(std::lround(dead * n)), n);
        const int redN = qBound(0, static_cast<int>(std::lround(inf * n)), n - blackN);

        p->setPen(Qt::NoPen);
        p->setBrush(bdot);
        for (int i = 0; i < blackN; ++i)
            p->drawEllipse(pool[i], m_dotRadius, m_dotRadius);
        p->setBrush(rdot);
        for (int i = blackN; i < blackN + redN; ++i)
            p->drawEllipse(pool[i], m_dotRadius, m_dotRadius);
    }
}

void MapWidget::onRegionUpdated(int regionId, RegionData data)
{
    if (regionId < 0 || regionId >= m_regionInf.size())
        return;
    const double total = static_cast<double>(data.totalPopulation);
    const double inf = total > 0 ? qBound(0.0, data.infectedCount / total, 1.0) : 0.0;
    const double dead = total > 0 ? qBound(0.0, data.deadCount / total, 1.0) : 0.0;
    // 海空载毒概率用“感染/存活”比例；存活为 0 时取 0，避免除以 0。
    const double alive = static_cast<double>(data.alive());
    const double infAlive = alive > 0 ? qBound(0.0, data.infectedCount / alive, 1.0) : 0.0;
    m_regionInf[regionId] = inf;
    m_regionInfAlive[regionId] = infAlive;
    m_regionDead[regionId] = dead;
    if (m_dotsLayer)
        m_dotsLayer->update(); // 触发点阵重绘（红/黑点密度变化）
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
    bubble->setOffset(-lw / 2.0, -lh / 2.0); // 图标居中对准地区中心
    bubble->setPos(c);
    bubble->setZValue(100);
    bubble->setToolTip(QString("点击收集 %1 DNA").arg(value));
    m_bubbleValue[bubble] = value;
    // 寿命以“游戏天数”计：暂停时（含进入菜单）不流逝，倍速时更快消失
    m_bubbleLife[bubble] = kBubbleLifeDays;

    SoundManager::instance().playSfx(SoundManager::BubbleSpawn); // 气泡产生音效
}

// 每过一天递减气泡寿命，到期则移除（受倍速/暂停影响）
void MapWidget::onDayTick(int)
{
    QVector<QGraphicsItem *> expired;
    for (auto it = m_bubbleLife.begin(); it != m_bubbleLife.end(); ++it)
        if (--it.value() <= 0)
            expired.append(it.key());
    for (QGraphicsItem *b : expired) {
        m_bubbleLife.remove(b);
        m_bubbleValue.remove(b);
        m_scene->removeItem(b);
        delete b;
    }

    // 每个游戏日：按各机场/港口的平均发车间隔投放航班/航次
    dispatchDepartures();
}

// 重开：清空所有 DNA 气泡、开始气泡、点阵密度与选区
void MapWidget::resetMap()
{
    const QList<QGraphicsItem *> bubbles = m_bubbleValue.keys();
    for (QGraphicsItem *b : bubbles) {
        m_scene->removeItem(b);
        delete b;
    }
    m_bubbleValue.clear();
    m_bubbleLife.clear();
    // 机场/港口为永久固定设施：重开（含换地区）不清除、不重载，保持原样。
    // 但在途载具与“感染航线”属于一局游戏的动态内容，重开时清除（航线几何/网格保留）。
    for (Vehicle &v : m_vehicles) {
        if (v.item) { m_scene->removeItem(v.item); delete v.item; }
    }
    m_vehicles.clear();
    for (auto it = m_routeVis.begin(); it != m_routeVis.end(); ++it) {
        if (it.value().item) { m_scene->removeItem(it.value().item); delete it.value().item; }
    }
    m_routeVis.clear();
    // 解除全部封锁标记并复原设施图标亮度
    m_airportClosed.clear();
    m_portClosed.clear();
    for (const Facility &f : m_facilities) {
        if (f.item) f.item->setOpacity(1.0);
        if (f.cross) f.cross->setVisible(false);
    }
    clearStartBubble();
    m_seeded = false;
    selectRegion(-1);
    m_regionInf.fill(0.0, m_regionInf.size());
    m_regionInfAlive.fill(0.0, m_regionInfAlive.size());
    m_regionDead.fill(0.0, m_regionDead.size());
    if (m_dotsLayer)
        m_dotsLayer->update();
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
            item->setPen(kBorderPen);
    }
    m_selectedRegion = regionId;
    if (regionId >= 0 && regionId < m_regionItems.size()) {
        for (QGraphicsPathItem *item : m_regionItems[regionId])
            item->setPen(kHighlightPen); // 黄色高亮
    }
}

// 真正的“点击”（按下到松开几乎没有移动）才触发：选区 / 收集气泡 / 投放
void MapWidget::handleClickAt(const QPoint &viewPos)
{
    QGraphicsItem *it = itemAt(viewPos);
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
            m_bubbleLife.remove(it); // 同时移除寿命记录，避免到期时重复删除（悬空指针）
            m_scene->removeItem(it);
            delete it;
            SoundManager::instance().playSfx(SoundManager::BubblePop); // 气泡点击音效
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
    // 未精确命中陆地：在容差范围内吸附到最近的地区，再判海洋。
    // 这样可修复个别国家矢量轮廓偏小/缺失（如南苏丹未单独绘制、并入苏丹辖区）、
    // 以及卫星底图与矢量轮廓存在轻微横向偏移导致“看得到却点不到”的问题。
    {
        const QPointF sp = mapToScene(viewPos);
        int region = -1;
        QPointF boundary;
        bool inside = false;
        if (pickFacilityTarget(sp, region, boundary, inside)) {
            selectRegion(region);
            emit regionSelected(region);
            if (!m_seeded)
                showStartBubble(region);
            return;
        }
    }
    // 点到深海/空白
    selectRegion(-1);
    emit oceanClicked();
}

// ——— 设施放置：几何工具 ———
// 点到矩形的距离（点在矩形内为 0）
static double distPointToRect(const QPointF &p, const QRectF &r)
{
    const double dx = qMax(qMax(r.left() - p.x(), p.x() - r.right()), 0.0);
    const double dy = qMax(qMax(r.top() - p.y(), p.y() - r.bottom()), 0.0);
    return std::hypot(dx, dy);
}
// 线段 ab 上离 p 最近的点，d2 返回最近距离的平方
static QPointF nearestOnSeg(const QPointF &a, const QPointF &b, const QPointF &p, double &d2)
{
    const QPointF ab = b - a;
    const double L2 = ab.x() * ab.x() + ab.y() * ab.y();
    double t = (L2 > 0.0) ? ((p - a).x() * ab.x() + (p - a).y() * ab.y()) / L2 : 0.0;
    t = qBound(0.0, t, 1.0);
    const QPointF q = a + ab * t;
    const QPointF d = p - q;
    d2 = d.x() * d.x() + d.y() * d.y();
    return q;
}

// 地区所有国家轮廓上离 sp 最近的点（场景坐标）。国家图元无位移/变换，path 即场景坐标。
QPointF MapWidget::nearestBoundaryPoint(int region, const QPointF &sp) const
{
    QPointF best = sp;
    double bestD2 = std::numeric_limits<double>::max();
    if (region < 0 || region >= m_regionItems.size())
        return best;
    for (QGraphicsPathItem *item : m_regionItems[region]) {
        const QList<QPolygonF> polys = item->path().toSubpathPolygons();
        for (const QPolygonF &poly : polys) {
            for (int k = 0; k + 1 < poly.size(); ++k) {
                double d2;
                const QPointF q = nearestOnSeg(poly[k], poly[k + 1], sp, d2);
                if (d2 < bestD2) { bestD2 = d2; best = q; }
            }
        }
    }
    return best;
}

// 决定右键点应归属哪个地区：
//  1) 若 sp 落在某地区陆地内 -> 该地区，inside=true，boundary=其海岸最近点（供港口吸附）；
//  2) 否则取“边界离 sp 最近”的地区（点在近海时仍可建），inside=false；
//     若最近边界仍超过吸附上限，视为深海，返回 false（不弹窗）。
bool MapWidget::pickFacilityTarget(const QPointF &sp, int &region, QPointF &boundary, bool &inside) const
{
    // —— 1) 陆地内 —— （先按包围盒粗筛，再精确判定 path.contains）
    for (auto it = m_itemRegion.constBegin(); it != m_itemRegion.constEnd(); ++it) {
        QGraphicsPathItem *item = static_cast<QGraphicsPathItem *>(it.key());
        if (!item->sceneBoundingRect().contains(sp))
            continue;
        if (item->path().contains(sp)) {
            region = it.value();
            inside = true;
            boundary = nearestBoundaryPoint(region, sp);
            return true;
        }
    }

    // —— 2) 近海：找边界最近的地区 ——
    const double kMaxSnap = 60.0; // 场景单位（整图宽约 2752）；超过即认为是远海
    double bestD2 = std::numeric_limits<double>::max();
    int bestRegion = -1;
    QPointF bestPt;
    for (auto it = m_itemRegion.constBegin(); it != m_itemRegion.constEnd(); ++it) {
        QGraphicsPathItem *item = static_cast<QGraphicsPathItem *>(it.key());
        const QRectF bb = item->sceneBoundingRect();
        if (distPointToRect(sp, bb) > std::sqrt(bestD2)) // 包围盒已比当前最优远 -> 跳过精算
            continue;
        const QList<QPolygonF> polys = item->path().toSubpathPolygons();
        for (const QPolygonF &poly : polys) {
            for (int k = 0; k + 1 < poly.size(); ++k) {
                double d2;
                const QPointF q = nearestOnSeg(poly[k], poly[k + 1], sp, d2);
                if (d2 < bestD2) { bestD2 = d2; bestPt = q; bestRegion = it.value(); }
            }
        }
    }
    if (bestRegion >= 0 && std::sqrt(bestD2) <= kMaxSnap) {
        region = bestRegion;
        boundary = bestPt;
        inside = false;
        return true;
    }
    return false;
}

// 右键：定位国家 -> 弹窗（机场/港口/取消）-> 放置
void MapWidget::handleRightClickAt(const QPoint &viewPos)
{
    const QPointF sp = mapToScene(viewPos);
    int region = -1;
    QPointF boundary;
    bool inside = false;
    if (!pickFacilityTarget(sp, region, boundary, inside))
        return; // 远海空白：不弹窗

    const QString name = Regions::names().value(region);
    QMessageBox box(this);
    box.setWindowTitle("建立设施");
    box.setText(QString("在【%1】建立：").arg(name));
    QPushButton *btnAir = box.addButton("机场", QMessageBox::AcceptRole);
    QPushButton *btnPort = box.addButton("港口", QMessageBox::AcceptRole);
    box.addButton("取消", QMessageBox::RejectRole);
    box.exec();

    QAbstractButton *chosen = box.clickedButton();
    if (chosen == btnAir) {
        // 机场在陆地内部：点在陆地内则用原点，否则退回到海岸点
        addFacility(0, region, inside ? sp : boundary);
    } else if (chosen == btnPort) {
        // 港口吸附到海岸线（便于日后绘制航线）
        addFacility(1, region, boundary);
    }
}

// 放置一个设施图标（场景尺寸固定，随地图缩放等比变化；居中锚定在目标点）。
// persist=true 时同步写入存档（玩家放置）；从存档载入时传 false 避免回写。
void MapWidget::addFacility(int type, int region, const QPointF &scenePos, bool persist)
{
    // 已停用设施：仍加入 m_facilities 以保持“编号==索引”对齐，但图标隐藏、也不参与航线
    const int newIndex = m_facilities.size();
    const bool disabled = Transport::disabledFacilities().contains(newIndex);

    const QPixmap &pm = (type == 0) ? m_iconAirport : m_iconPort;
    const qreal lw = pm.width(), lh = pm.height(); // 贴图 dpr=1，像素=逻辑尺寸

    QGraphicsPixmapItem *item = m_scene->addPixmap(pm);
    item->setOffset(-lw / 2.0, -lh / 2.0); // 图标中心对准目标点（缩放/旋转绕原点=中心）
    // 场景尺寸固定：贴图最长边映射为 kFacilitySceneSize；不忽略变换，故随地图缩放等比变化
    const double texMax = qMax(1.0, qMax(lw, lh));
    item->setScale(kFacilitySceneSize / texMax);
    item->setPos(scenePos);
    if (disabled)
        item->setVisible(false); // 不再显示该港口
    item->setZValue(80);                    // 国界(0)/点阵(50) 之上，DNA 气泡(100+) 之下
    item->setAcceptedMouseButtons(Qt::NoButton);
    const QString name = Regions::names().value(region);
    item->setToolTip(QString("%1 · %2").arg(type == 0 ? "机场" : "港口", name));

    // 封锁红叉：图标子项（随图标一起缩放/移动），默认隐藏；尺寸落在图标范围内。
    const double xa = 0.40 * qMin(lw, lh);
    QPainterPath xp;
    xp.moveTo(-xa, -xa); xp.lineTo(xa, xa);
    xp.moveTo(-xa,  xa); xp.lineTo(xa, -xa);
    QGraphicsPathItem *cross = new QGraphicsPathItem(xp, item);
    QPen xpen(QColor(225, 20, 24), qMax(2.0, 0.14 * qMin(lw, lh)));
    xpen.setCapStyle(Qt::RoundCap);
    cross->setPen(xpen);
    cross->setZValue(1.0);                                       // 在图标贴图之上
    cross->setFlag(QGraphicsItem::ItemIgnoresParentOpacity, true); // 图标变暗时红叉仍醒目
    cross->setAcceptedMouseButtons(Qt::NoButton);
    cross->setVisible(false);

    m_facilities.push_back({type, region, scenePos, item, cross});
    if (persist)
        saveFacilities();
}

// 存档路径：与可执行文件同目录的 facilities.txt（同一构建跨次运行稳定保留）
QString MapWidget::facilitiesFilePath() const
{
    return QCoreApplication::applicationDirPath() + "/facilities.txt";
}

// 写出全部设施（每行：type x y region）。每次放置后调用，做到“永久保存”。
void MapWidget::saveFacilities() const
{
    QFile f(facilitiesFilePath());
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qDebug() << "设施存档写入失败：" << facilitiesFilePath();
        return;
    }
    QTextStream ts(&f);
    for (const Facility &fa : m_facilities)
        ts << fa.type << ' ' << fa.pos.x() << ' ' << fa.pos.y() << ' ' << fa.region << '\n';
    f.close();
    writeFacilityIndex(); // 同步刷新编号表
}

// 导出设施编号表 facilities_index.txt：每行 “序号  类型  地区编号  地区名”。
// 序号从 0 起，即该设施在航线权重图中的节点 ID（机场与港口统一编号）。
void MapWidget::writeFacilityIndex() const
{
    QFile f(QCoreApplication::applicationDirPath() + "/facilities_index.txt");
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qDebug() << "设施编号表写入失败";
        return;
    }
    QTextStream ts(&f); // Qt6 默认 UTF-8，中文地区名可正常写出
    ts << "# 设施编号表（航线权重图节点ID，序号从0起；机场与港口统一编号）\n";
    ts << "# 序号\t类型\t地区编号\t地区名\n";
    for (int i = 0; i < m_facilities.size(); ++i) {
        const Facility &fa = m_facilities[i];
        ts << i << '\t' << (fa.type == 0 ? "机场" : "港口") << '\t'
           << fa.region << '\t' << Regions::names().value(fa.region) << '\n';
    }
}

// 启动时从存档载入设施（不回写）。文件不存在则无设施。
void MapWidget::loadFacilities()
{
    QFile f(facilitiesFilePath());
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
        return;
    QTextStream ts(&f);
    int loaded = 0;
    while (!ts.atEnd()) {
        const QStringList p = ts.readLine().split(' ', Qt::SkipEmptyParts);
        if (p.size() < 4)
            continue;
        addFacility(p[0].toInt(), p[3].toInt(), QPointF(p[1].toDouble(), p[2].toDouble()), false);
        ++loaded;
    }
    qDebug() << "已载入固定设施：" << loaded << "个 <-" << facilitiesFilePath();
    if (loaded > 0)
        writeFacilityIndex(); // 启动即同步编号表（与存档一致）
}

void MapWidget::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::RightButton) {
        // 右键：在该国建立机场/港口（不参与平移/选区）
        handleRightClickAt(event->pos());
        event->accept();
        return;
    }
    if (event->button() == Qt::LeftButton) {
        // 先记录起点，选区/拖拽留到 move/release 再判定（区分点击与拖动）
        m_panning = true;
        m_dragMoved = false;
        m_pressPos = event->pos();
        m_lastPanPos = event->pos();
        event->accept();
        return;
    }
    QGraphicsView::mousePressEvent(event);
}

void MapWidget::mouseMoveEvent(QMouseEvent *event)
{
    if (m_panning && (event->buttons() & Qt::LeftButton)) {
        if (!m_dragMoved &&
            (event->pos() - m_pressPos).manhattanLength() > 4) // 超过阈值才算拖拽
            m_dragMoved = true;
        if (m_dragMoved) {
            const QPoint d = event->pos() - m_lastPanPos;
            m_lastPanPos = event->pos();
            m_userZoomed = true; // 拖拽视为手动导航，不再自动铺满
            viewport()->setCursor(Qt::ClosedHandCursor);
            // 用滚动条平移视图（滚动条策略为 AlwaysOff，仅隐藏，仍可程序化滚动）
            horizontalScrollBar()->setValue(horizontalScrollBar()->value() - d.x());
            verticalScrollBar()->setValue(verticalScrollBar()->value() - d.y());
        }
        event->accept();
        return;
    }
    QGraphicsView::mouseMoveEvent(event);
}

void MapWidget::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && m_panning) {
        m_panning = false;
        viewport()->unsetCursor();
        if (!m_dragMoved) // 没移动 -> 当作点击
            handleClickAt(event->pos());
        event->accept();
        return;
    }
    QGraphicsView::mouseReleaseEvent(event);
}

// 当前视口下“铺满全图”所需的缩放值（与 fitInView(KeepAspectRatio) 一致）
double MapWidget::fitScale() const
{
    const QRectF r = m_fitRect.isEmpty() ? m_scene->itemsBoundingRect() : m_fitRect;
    const double vw = viewport()->width(), vh = viewport()->height();
    if (r.width() <= 0 || r.height() <= 0 || vw <= 0 || vh <= 0)
        return 1.0;
    return qMin(vw / r.width(), vh / r.height());
}

// 若当前缩放比最小允许值还小，则拉回到最小值
void MapWidget::enforceMinZoom()
{
    const double minS = fitScale() * m_minZoomFactor;
    const double cur = transform().m11();
    if (cur > 0.0 && cur < minS - 1e-9)
        scale(minS / cur, minS / cur);
}

void MapWidget::wheelEvent(QWheelEvent *event)
{
    m_userZoomed = true; // 用户手动缩放后，不再自动适配
    double factor = (event->angleDelta().y() > 0) ? 1.15 : 0.87;
    const double cur = transform().m11();
    const double minS = fitScale() * m_minZoomFactor;
    if (factor < 1.0 && cur * factor < minS) // 缩小时不得突破最小缩放
        factor = (cur > 0.0) ? minS / cur : 1.0;
    if (factor > 0.0 && qAbs(factor - 1.0) > 1e-6)
        scale(factor, factor);
}

// 默认视图（已定标）：缩放到最小缩放（铺满×系数），并定位到固定中心（归一化位置）。
// 仅在用户尚未手动平移/缩放时生效，使每次进入地图都框在同一块局部。
void MapWidget::applyDefaultView()
{
    if (m_userZoomed || m_scene->items().isEmpty() || m_fitRect.isEmpty())
        return;
    auto frame = [this]() {
        if (m_userZoomed || m_fitRect.isEmpty() || viewport()->width() <= 0)
            return;
        resetTransform();
        const double s = fitScale() * m_minZoomFactor;
        if (s > 0.0)
            scale(s, s);
        const QPointF c(m_fitRect.left() + kViewCenterNX * m_fitRect.width(),
                        m_fitRect.top()  + kViewCenterNY * m_fitRect.height());
        centerOn(c); // 超出可视范围会自动夹取到边界内
    };
    frame();
    // 视口尺寸在 show/resize 时可能还未最终确定，延迟一帧再定位一次
    QTimer::singleShot(0, this, frame);
}

void MapWidget::showEvent(QShowEvent *event)
{
    QGraphicsView::showEvent(event);
    applyDefaultView();
}

void MapWidget::resizeEvent(QResizeEvent *event)
{
    QGraphicsView::resizeEvent(event);
    applyDefaultView();   // 未手动操作过：回到默认框
    enforceMinZoom();     // 已手动操作过：窗口变化后至少不低于最小缩放
}

// 依当前校准参数（平移 + 分轴缩放，以整幅中心为锚点）重新摆放卫星底图
void MapWidget::applySatCalibration()
{
    if (!m_satItem)
        return;
    const double sx = (m_satViewBox.width() / m_satPixW) * m_satScaleX;
    const double sy = (m_satViewBox.height() / m_satPixH) * m_satScaleY;
    m_satItem->setTransform(QTransform::fromScale(sx, sy));
    const double imgW = m_satPixW * sx;
    const double imgH = m_satPixH * sy;
    const QPointF c = m_satViewBox.center();
    m_satItem->setPos(c.x() - imgW / 2.0 + m_satOffX,
                      c.y() - imgH / 2.0 + m_satOffY);
}

// =====================================================================
//  海空交通系统：固定航线 + 动画载具 + 受感染红色虚线航线
// =====================================================================

// 疾病属性变化（来自 GameCore）：用于海空“载毒概率”计算
//   载毒概率 = 出发国感染比例 × 传染性 × 海/空途径乘区
// 其中海/空途径乘区随【空气/水源传播】技能提升（GameCore 已算好，随属性下发）。
void MapWidget::onDiseaseStatsChanged(DiseaseStats stats)
{
    m_infectivity = stats.infectivity;
    m_routeModifier = stats.routeModifier;
}

// 游戏速度变化（来自 GameCore::setSpeed）：让海空载具与游戏时间同步。
//   intervalMs<=0 表示暂停（含进入菜单）-> 倍速 0 -> 载具冻结；
//   否则倍速 = kBaseDayMs / intervalMs（1000ms=1×, 500ms=2× …）。
void MapWidget::onGameSpeedChanged(int intervalMs)
{
    m_speedFactor = (intervalMs <= 0)
                        ? 0.0
                        : GameParams::kBaseDayMs / static_cast<double>(intervalMs);
}

// 封锁状态变化：记录该地区机场/港口是否封锁，并把对应设施图标变暗/复原
void MapWidget::onLockdownChanged(int regionId, bool airportClosed, bool portClosed, bool /*borderClosed*/)
{
    if (airportClosed) m_airportClosed.insert(regionId); else m_airportClosed.remove(regionId);
    if (portClosed)    m_portClosed.insert(regionId);    else m_portClosed.remove(regionId);
    applyFacilityLockVisual(regionId);
}

// 按封锁状态设置该地区设施图标透明度（封锁=变暗）
void MapWidget::applyFacilityLockVisual(int region)
{
    for (const Facility &f : m_facilities) {
        if (f.region != region || !f.item)
            continue;
        const bool closed = (f.type == 0) ? m_airportClosed.contains(region)
                                          : m_portClosed.contains(region);
        f.item->setOpacity(closed ? 0.5 : 1.0);
        if (f.cross)
            f.cross->setVisible(closed); // 封锁 -> 图标上显示红叉
    }
}

// 载具贴图：原图为白底（plane/ship 为白机白船带细描边，red_* 为红机红船）。
// 从四周边界对“白色背景”做洪水填充并置为透明（保留被描边围住的机身内部白色），
// 这样白机/红船都能干净地贴在地图上。烘焙为固定分辨率，按场景尺寸摆放（随地图缩放）。
QPixmap MapWidget::makeVehiclePixmap(const QString &res) const
{
    using namespace GameParams;
    QImage img(res);
    if (img.isNull())
        return QPixmap();
    img = img.convertToFormat(QImage::Format_ARGB32);
    const int w = img.width(), h = img.height();
    const int thr = 234; // 视为“背景白”的阈值
    QVector<quint8> bg(w * h, 0);
    QQueue<int> q;
    auto isWhite = [&](int x, int y) {
        const QRgb c = img.pixel(x, y);
        return qRed(c) >= thr && qGreen(c) >= thr && qBlue(c) >= thr;
    };
    auto tryPush = [&](int x, int y) {
        if (x < 0 || y < 0 || x >= w || y >= h) return;
        const int id = y * w + x;
        if (bg[id]) return;
        if (!isWhite(x, y)) return;
        bg[id] = 1;
        q.enqueue(id);
    };
    for (int x = 0; x < w; ++x) { tryPush(x, 0); tryPush(x, h - 1); }
    for (int y = 0; y < h; ++y) { tryPush(0, y); tryPush(w - 1, y); }
    while (!q.isEmpty()) {
        const int id = q.dequeue();
        const int x = id % w, y = id / w;
        tryPush(x + 1, y); tryPush(x - 1, y); tryPush(x, y + 1); tryPush(x, y - 1);
    }
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x)
            if (bg[y * w + x])
                img.setPixelColor(x, y, QColor(0, 0, 0, 0));

    // 烘焙为固定像素分辨率（dpr=1）；最终大小由 spawnVehicle 按场景尺寸 setScale 决定，
    // 使载具随地图缩放等比变化、放大后依然清晰。
    const int side = qMax(1, qRound(kVehicleTexPx));
    QPixmap pm = QPixmap::fromImage(img).scaled(side, side,
                                                Qt::KeepAspectRatio, Qt::SmoothTransformation);
    return pm;
}

// 交通系统初始化：贴图 / 海洋寻路网格 / 预计算全部固定航线 / 启动动画定时器
void MapWidget::initTransport()
{
    using namespace GameParams;
    m_icoPlane    = makeVehiclePixmap(":/graph/plane.jpg");
    m_icoShip     = makeVehiclePixmap(":/graph/ship.jpg");
    m_icoPlaneRed = makeVehiclePixmap(":/graph/red_plane.jpg");
    m_icoShipRed  = makeVehiclePixmap(":/graph/red_ship.jpg");
    m_routeModifier = kRouteModifierBase; // 海/空途径乘区（默认极低，后续可随技能提升）

    buildNavGrid();
    precomputeRoutes();

    m_transTimer = new QTimer(this);
    m_transTimer->setInterval(33); // ~30FPS，平滑移动（每帧实时步进）
    connect(m_transTimer, &QTimer::timeout, this, [this]() {
        static QElapsedTimer clk;
        if (!clk.isValid()) { clk.start(); m_lastAnimMs = 0; }
        const qint64 now = clk.elapsed();
        double dt = (now - m_lastAnimMs) / 1000.0;
        m_lastAnimMs = now;
        if (dt < 0) dt = 0;
        if (dt > 0.1) dt = 0.1;          // 防止卡顿后大跳
        dt *= m_speedFactor;             // 随游戏加速/暂停同步：暂停(含进入菜单)时 m_speedFactor=0 -> 冻结
        if (dt > 0.0) stepVehicles(dt);
    });
    m_transTimer->start();
}

// 由国家形状栅格化出“海/陆”掩膜（低分辨率），并叠加北冰洋/南极禁航带。
void MapWidget::buildNavGrid()
{
    using namespace GameParams;
    m_navRect = QRectF(0, 0, 2752.766, 1537.631); // 与 viewBox 一致（国家坐标系）
    m_navCell = kNavCell;
    m_navCols = qMax(1, static_cast<int>(std::ceil(m_navRect.width() / m_navCell)));
    m_navRows = qMax(1, static_cast<int>(std::ceil(m_navRect.height() / m_navCell)));

    QImage img(m_navCols, m_navRows, QImage::Format_Grayscale8);
    img.fill(255); // 白=海
    {
        QPainter p(&img);
        p.setRenderHint(QPainter::Antialiasing, false);
        p.scale(m_navCols / m_navRect.width(), m_navRows / m_navRect.height());
        p.translate(-m_navRect.topLeft());
        p.setPen(Qt::NoPen);
        p.setBrush(Qt::black); // 黑=陆地
        for (auto it = m_countryItems.constBegin(); it != m_countryItems.constEnd(); ++it)
            p.drawPath(it.value()->path());
    }

    const int N = m_navCols * m_navRows;
    // 分别记录“栅格化陆地”与“极区禁航带”：极区是硬边界（不参与海洋膨胀，免得船驶入北冰洋）。
    QVector<uchar> land(N, 0), pole(N, 0);
    for (int r = 0; r < m_navRows; ++r) {
        const uchar *line = img.constScanLine(r);
        const double cy = m_navRect.top() + (r + 0.5) * m_navCell;
        const bool poleN = cy < kNavArcticY;
        const bool poleS = cy > kNavAntarcticY;
        for (int c = 0; c < m_navCols; ++c) {
            land[r * m_navCols + c] = (line[c] < 128) ? 1 : 0;
            pole[r * m_navCols + c] = (poleN || poleS) ? 1 : 0;
        }
    }

    const int dcs8[8] = { 1, -1, 0, 0, 1, 1, -1, -1 };
    const int drs8[8] = { 0, 0, 1, -1, 1, -1, 1, -1 };

    // 形态学“海洋膨胀”：贴着开阔海（非陆非极）的薄陆地逐圈打通，连通窄口/封闭海/小水塘。
    for (int it = 0; it < kNavErodeCells; ++it) {
        QVector<uchar> next = land;
        for (int r = 0; r < m_navRows; ++r)
            for (int c = 0; c < m_navCols; ++c) {
                const int i = r * m_navCols + c;
                if (!land[i]) continue;
                bool nearSea = false;
                for (int k = 0; k < 8 && !nearSea; ++k) {
                    int nc = c + dcs8[k], nr = r + drs8[k];
                    if (nr < 0 || nr >= m_navRows) continue;
                    if (kNavWrapX) nc = ((nc % m_navCols) + m_navCols) % m_navCols;
                    else if (nc < 0 || nc >= m_navCols) continue;
                    const int ni = nr * m_navCols + nc;
                    if (!land[ni] && !pole[ni]) nearSea = true; // 邻接开阔海
                }
                if (nearSea) next[i] = 0; // 这格薄陆地被海“吃掉”
            }
        land.swap(next);
    }

    m_navBlocked.fill(0, N);
    for (int i = 0; i < N; ++i)
        m_navBlocked[i] = (land[i] || pole[i]) ? 1 : 0;

    // 距陆/极距离场（格）：多源 BFS（8 邻接，含东西绕图），让航线尽量远离海岸线。
    const int CAP = 64;
    m_navClear.fill(CAP, N);
    QQueue<int> bfs;
    for (int i = 0; i < N; ++i)
        if (m_navBlocked[i]) { m_navClear[i] = 0; bfs.enqueue(i); }
    while (!bfs.isEmpty()) {
        const int cur = bfs.dequeue();
        const int cc = cur % m_navCols, cr = cur / m_navCols;
        const int nd = m_navClear[cur] + 1;
        if (nd >= CAP) continue;
        for (int k = 0; k < 8; ++k) {
            int nc = cc + dcs8[k], nr = cr + drs8[k];
            if (nr < 0 || nr >= m_navRows) continue;
            if (kNavWrapX) nc = ((nc % m_navCols) + m_navCols) % m_navCols;
            else if (nc < 0 || nc >= m_navCols) continue;
            const int ni = nr * m_navCols + nc;
            if (nd < m_navClear[ni]) { m_navClear[ni] = nd; bfs.enqueue(ni); }
        }
    }

    // 连通水域标号（8 邻接，含东西绕图）+ 各域格数：吸附时只认“大洋”，排除封闭小水塘/内陆湖。
    m_navComp.fill(-1, N);
    m_navCompSize.clear();
    QQueue<int> cq;
    for (int seed = 0; seed < N; ++seed) {
        if (m_navBlocked[seed] || m_navComp[seed] != -1) continue;
        const int cid = m_navCompSize.size();
        int sz = 0;
        m_navComp[seed] = cid;
        cq.enqueue(seed);
        while (!cq.isEmpty()) {
            const int cur = cq.dequeue();
            ++sz;
            const int cc = cur % m_navCols, cr = cur / m_navCols;
            for (int k = 0; k < 8; ++k) {
                int nc = cc + dcs8[k], nr = cr + drs8[k];
                if (nr < 0 || nr >= m_navRows) continue;
                if (kNavWrapX) nc = ((nc % m_navCols) + m_navCols) % m_navCols;
                else if (nc < 0 || nc >= m_navCols) continue;
                const int ni = nr * m_navCols + nc;
                if (m_navBlocked[ni] || m_navComp[ni] != -1) continue;
                m_navComp[ni] = cid;
                cq.enqueue(ni);
            }
        }
        m_navCompSize.push_back(sz);
    }
    qDebug() << "海洋寻路网格：" << m_navCols << "x" << m_navRows
             << " 连通水域" << m_navCompSize.size();
}

// 在海洋网格上用 A* 求两港口间的固定航线（避陆/避北冰洋/避底缘；东西可绕图）。
// 返回“展开”坐标折线（跨太平洋时 x 会超出图宽，由显示端取模回绕）。失败返回空。
QVector<QPointF> MapWidget::computeShipPath(const QPointF &a, const QPointF &b) const
{
    using namespace GameParams;
    QVector<QPointF> fail;
    if (m_navCols <= 0 || m_navRows <= 0)
        return fail;
    const int cols = m_navCols, rows = m_navRows;
    const double cell = m_navCell;

    auto blocked = [&](int c, int r) { return m_navBlocked[r * cols + c] != 0; };
    // 开阔海：可航行 且 所属连通水域足够大（排除栅格化封闭小水塘/内陆湖，避免被困其中走直线穿陆）
    auto openOcean = [&](int c, int r) {
        const int id = r * cols + c;
        if (m_navBlocked[id]) return false;
        const int cid = m_navComp[id];
        return cid >= 0 && m_navCompSize[cid] >= kNavMinOceanCells;
    };
    auto cellOf = [&](const QPointF &p, int &c, int &r) {
        c = static_cast<int>((p.x() - m_navRect.left()) / cell);
        r = static_cast<int>((p.y() - m_navRect.top()) / cell);
        c = qBound(0, c, cols - 1);
        r = qBound(0, r, rows - 1);
    };
    // 港口常落在陆地格或封闭小水塘：向四周环形搜索最近的“开阔海”格
    auto snap = [&](int &c, int &r) {
        if (openOcean(c, r)) return true;
        for (int rad = 1; rad <= kNavSnapRadius; ++rad) {
            for (int dr = -rad; dr <= rad; ++dr)
                for (int dc = -rad; dc <= rad; ++dc) {
                    if (qAbs(dr) != rad && qAbs(dc) != rad) continue; // 仅取最外环
                    int nc = c + dc, nr = r + dr;
                    if (kNavWrapX) nc = ((nc % cols) + cols) % cols;
                    if (nc < 0 || nc >= cols || nr < 0 || nr >= rows) continue;
                    if (openOcean(nc, nr)) { c = nc; r = nr; return true; }
                }
        }
        return false;
    };

    int sc, sr, gc, gr;
    cellOf(a, sc, sr);
    cellOf(b, gc, gr);
    if (!snap(sc, sr) || !snap(gc, gr))
        return fail;
    if (sc == gc && sr == gr)
        return QVector<QPointF>{ a, b };

    const int N = cols * rows;
    std::vector<float> dist(N, std::numeric_limits<float>::max());
    std::vector<int> came(N, -1);
    auto idx = [&](int c, int r) { return r * cols + c; };
    auto heur = [&](int c, int r) {
        int dx = qAbs(c - gc);
        if (kNavWrapX) dx = qMin(dx, cols - dx);
        const int dy = qAbs(r - gr);
        return static_cast<float>(std::sqrt(static_cast<double>(dx * dx + dy * dy)));
    };

    typedef std::pair<float, int> PQ;
    std::priority_queue<PQ, std::vector<PQ>, std::greater<PQ>> open;
    const int s = idx(sc, sr), g = idx(gc, gr);
    dist[s] = 0.0f;
    open.push({ heur(sc, sr), s });

    const int dcs[8] = { 1, -1, 0, 0, 1, 1, -1, -1 };
    const int drs[8] = { 0, 0, 1, -1, 1, -1, 1, -1 };
    while (!open.empty()) {
        const PQ top = open.top();
        open.pop();
        const int cur = top.second;
        const int cc = cur % cols, cr = cur / cols;
        if (top.first - heur(cc, cr) > dist[cur] + 1e-3f)
            continue; // 过期堆项
        if (cur == g)
            break;
        for (int k = 0; k < 8; ++k) {
            int nc = cc + dcs[k], nr = cr + drs[k];
            if (nr < 0 || nr >= rows) continue;
            if (kNavWrapX) nc = ((nc % cols) + cols) % cols;
            else if (nc < 0 || nc >= cols) continue;
            if (blocked(nc, nr)) continue;
            if (dcs[k] != 0 && drs[k] != 0) { // 对角不可切陆角
                int ac = cc + dcs[k];
                if (kNavWrapX) ac = ((ac % cols) + cols) % cols;
                else if (ac < 0 || ac >= cols) continue;
                if (blocked(ac, cr) || blocked(cc, nr)) continue;
            }
            const float step = (dcs[k] != 0 && drs[k] != 0) ? 1.41421356f : 1.0f;
            const int ni = idx(nc, nr);
            // 离岸代价：离海岸越近（clear 越小）步进越贵 -> 航线偏向开阔水域、减少贴岸突变
            const int deficit = qMax(0, kNavClearTarget - m_navClear[ni]);
            const float coastW = 1.0f + static_cast<float>(kCoastPenalty * deficit);
            const float nd = dist[cur] + step * coastW;
            if (nd < dist[ni]) {
                dist[ni] = nd;
                came[ni] = cur;
                open.push({ nd + heur(nc, nr), ni });
            }
        }
    }
    if (came[g] < 0 && g != s)
        return fail; // 不可达

    QVector<int> cells;
    for (int cur = g; cur != -1; cur = came[cur]) {
        cells.push_back(cur);
        if (cur == s) break;
    }
    std::reverse(cells.begin(), cells.end());
    if (cells.isEmpty())
        return fail;

    // 展开（消除东西缝跳变），生成折线
    QVector<QPointF> poly;
    double offset = 0.0;
    int prevCol = cells.first() % cols;
    for (int i = 0; i < cells.size(); ++i) {
        const int c = cells[i] % cols, r = cells[i] / cols;
        if (kNavWrapX && i > 0) {
            const int d = c - prevCol;
            if (d > cols / 2) offset -= cols;
            else if (d < -cols / 2) offset += cols;
        }
        const double wx = m_navRect.left() + (c + offset + 0.5) * cell;
        const double wy = m_navRect.top() + (r + 0.5) * cell;
        poly.push_back(QPointF(wx, wy));
        prevCol = c;
    }
    if (poly.size() < 2)
        return fail;

    // 终点对齐到展开坐标系的同一“圈”（消除东西缝）
    const double endUnwrapX = poly.back().x();
    const double kk = std::round((endUnwrapX - b.x()) / m_navRect.width());
    const QPointF bU(b.x() + kk * m_navRect.width(), b.y());

    // 两端夹到“离港最近的海面点”：港口落在内陆/凹岸时，避免从港口引出一条穿过陆地的直线
    poly.front() = waterEdgeApproach(a,  poly[1]);
    poly.back()  = waterEdgeApproach(bU, poly[poly.size() - 2]);

    // 视线串拉简化 + 样条平滑：消除栅格折线的台阶感与生硬拐角
    return smoothShipPath(poly);
}

// 视线串拉（去除可直达的中间点）+ Catmull-Rom 逐段细分平滑（触陆的段退回直线）。
QVector<QPointF> MapWidget::smoothShipPath(const QVector<QPointF> &pts) const
{
    using namespace GameParams;
    if (pts.size() <= 2)
        return pts;

    // 1) 视线串拉：能保持离岸余量直达，就跳过中间点（消台阶/锯齿）
    const int losClear = 1;
    QVector<QPointF> simp;
    simp.push_back(pts.front());
    int anchor = 0;
    for (int i = 2; i < pts.size(); ++i) {
        if (!segNavigable(pts[anchor], pts[i], losClear)) {
            simp.push_back(pts[i - 1]);
            anchor = i - 1;
        }
    }
    simp.push_back(pts.back());
    if (simp.size() <= 2)
        return simp;

    // 2) Catmull-Rom 逐段细分；某段细分点触陆则该段退回直线（保证不穿陆）
    const int K = qMax(1, kSmoothSamplesPerSeg);
    QVector<QPointF> out;
    out.push_back(simp.front());
    for (int i = 0; i + 1 < simp.size(); ++i) {
        const QPointF p0 = simp[qMax(0, i - 1)];
        const QPointF p1 = simp[i];
        const QPointF p2 = simp[i + 1];
        const QPointF p3 = simp[qMin(simp.size() - 1, i + 2)];
        QVector<QPointF> seg;
        bool ok = true;
        for (int s = 1; s <= K; ++s) {
            const double t = static_cast<double>(s) / K, t2 = t * t, t3 = t2 * t;
            const double x = 0.5 * (2 * p1.x() + (-p0.x() + p2.x()) * t
                                    + (2 * p0.x() - 5 * p1.x() + 4 * p2.x() - p3.x()) * t2
                                    + (-p0.x() + 3 * p1.x() - 3 * p2.x() + p3.x()) * t3);
            const double y = 0.5 * (2 * p1.y() + (-p0.y() + p2.y()) * t
                                    + (2 * p0.y() - 5 * p1.y() + 4 * p2.y() - p3.y()) * t2
                                    + (-p0.y() + 3 * p1.y() - 3 * p2.y() + p3.y()) * t3);
            const QPointF q(x, y);
            if (s < K && navBlockedAt(q)) ok = false; // 末点(=p2)允许贴岸；中间点不得触陆
            seg.push_back(q);
        }
        if (ok)
            for (const QPointF &q : seg) out.push_back(q);
        else
            out.push_back(p2); // 退回该段直线，保持连续
    }
    return out;
}

// 某场景点是否禁航（越界视为禁航；带东西绕图）
bool MapWidget::navBlockedAt(const QPointF &p) const
{
    using namespace GameParams;
    if (m_navCols <= 0 || m_navRows <= 0) return true;
    int c = static_cast<int>(std::floor((p.x() - m_navRect.left()) / m_navCell));
    int r = static_cast<int>(std::floor((p.y() - m_navRect.top()) / m_navCell));
    if (kNavWrapX) c = ((c % m_navCols) + m_navCols) % m_navCols;
    else if (c < 0 || c >= m_navCols) return true;
    if (r < 0 || r >= m_navRows) return true;
    return m_navBlocked[r * m_navCols + c] != 0;
}

// 某场景点的离岸格距（越界视为 0）
int MapWidget::navClearAt(const QPointF &p) const
{
    using namespace GameParams;
    if (m_navCols <= 0 || m_navRows <= 0) return 0;
    int c = static_cast<int>(std::floor((p.x() - m_navRect.left()) / m_navCell));
    int r = static_cast<int>(std::floor((p.y() - m_navRect.top()) / m_navCell));
    if (kNavWrapX) c = ((c % m_navCols) + m_navCols) % m_navCols;
    else if (c < 0 || c >= m_navCols) return 0;
    if (r < 0 || r >= m_navRows) return 0;
    return m_navClear[r * m_navCols + c];
}

// 直线段是否全程可航（按 ~半格步进采样；minClear>0 时还需保持离岸余量）
bool MapWidget::segNavigable(const QPointF &a, const QPointF &b, int minClear) const
{
    const double len = std::hypot(b.x() - a.x(), b.y() - a.y());
    const int steps = qMax(1, static_cast<int>(std::ceil(len / (m_navCell * 0.5))));
    for (int s = 0; s <= steps; ++s) {
        const double t = static_cast<double>(s) / steps;
        const QPointF p(a.x() + (b.x() - a.x()) * t, a.y() + (b.y() - a.y()) * t);
        if (navBlockedAt(p)) return false;
        if (minClear > 0 && navClearAt(p) < minClear) return false;
    }
    return true;
}

// 从海面点 water 朝港口 port 走，返回仍在海面、最接近 port 的点（port 本身在海上则返回 port）
QPointF MapWidget::waterEdgeApproach(const QPointF &port, const QPointF &water) const
{
    const double len = std::hypot(port.x() - water.x(), port.y() - water.y());
    const int steps = qMax(1, static_cast<int>(std::ceil(len / (m_navCell * 0.5))));
    QPointF best = water;
    for (int s = 1; s <= steps; ++s) {
        const double t = static_cast<double>(s) / steps;
        const QPointF p(water.x() + (port.x() - water.x()) * t,
                        water.y() + (port.y() - water.y()) * t);
        if (navBlockedAt(p)) break;
        best = p;
    }
    return best;
}

// 依 transportdata 一次性预计算全部航线几何（固定不变，避免轨迹混乱）
void MapWidget::precomputeRoutes()
{
    m_routeGeom.clear();
    m_maxPlaneLen = 1.0;
    m_maxShipLen = 1.0;
    if (m_facilities.isEmpty()) {
        qDebug() << "无设施，跳过航线预计算";
        return;
    }
    const QVector<Transport::Node> &nodes = Transport::nodes();
    for (const Transport::Node &n : nodes) {
        if (n.id < 0 || n.id >= m_facilities.size())
            continue;
        for (const Transport::Edge &e : n.targets) {
            if (e.to < 0 || e.to >= m_facilities.size())
                continue;
            const qint64 key = pairKey(n.id, e.to);
            if (m_routeGeom.contains(key))
                continue;
            const int lo = qMin(n.id, e.to), hi = qMax(n.id, e.to);
            const QPointF pa = m_facilities[lo].pos, pb = m_facilities[hi].pos;
            QVector<QPointF> poly;
            if (n.type == 0) {
                poly = QVector<QPointF>{ pa, pb }; // 飞机走直线
            } else {
                poly = computeShipPath(pa, pb);    // 轮船走海洋
                if (poly.size() < 2) {
                    qDebug() << "轮船航线A*失败(直线兜底) 港口" << lo << "->" << hi
                             << "坐标" << pa << pb;
                    poly = QVector<QPointF>{ pa, pb }; // A* 失败兜底
                }
            }
            buildRouteGeom(key, n.type, poly);
        }
    }
    qDebug() << "航线预计算完成：" << m_routeGeom.size() << "条";
}

// 计算一条航线的累计弧长 / 总长 / 是否跨东西缝，并存入缓存
void MapWidget::buildRouteGeom(qint64 key, int type, const QVector<QPointF> &poly)
{
    RouteGeom g;
    g.type = type;
    g.poly = poly;
    g.cum.resize(poly.size());
    double L = 0.0;
    if (!poly.isEmpty())
        g.cum[0] = 0.0;
    for (int i = 1; i < poly.size(); ++i) {
        L += std::hypot(poly[i].x() - poly[i - 1].x(), poly[i].y() - poly[i - 1].y());
        g.cum[i] = L;
    }
    g.len = L;
    double minx = 1e18, maxx = -1e18;
    for (const QPointF &p : poly) { minx = qMin(minx, p.x()); maxx = qMax(maxx, p.x()); }
    g.wrapped = (maxx > m_navRect.right() + 1.0) || (minx < m_navRect.left() - 1.0);
    m_routeGeom.insert(key, g);
    if (type == 0) m_maxPlaneLen = qMax(m_maxPlaneLen, L);
    else           m_maxShipLen = qMax(m_maxShipLen, L);
}

// 每个游戏日：各机场/港口按平均发车间隔的概率投放航班/航次
void MapWidget::dispatchDepartures()
{
    using namespace GameParams;
    if (m_facilities.isEmpty())
        return;
    if (m_vehicles.size() >= kMaxVehicles)
        return;
    auto *rng = QRandomGenerator::global();
    const QVector<Transport::Node> &nodes = Transport::nodes();
    for (const Transport::Node &n : nodes) {
        if (n.id < 0 || n.id >= m_facilities.size())
            continue;
        if (n.targets.isEmpty() || n.avgDays <= 0)
            continue;
        // 出发地机场/港口被封锁 -> 该类设施停发载具（formula 形式三）
        const int srcRegion = m_facilities[n.id].region;
        if (n.type == 0 && m_airportClosed.contains(srcRegion))
            continue;
        if (n.type == 1 && m_portClosed.contains(srcRegion))
            continue;
        const double p = kTrafficScale / n.avgDays; // 平均每天发车概率
        if (rng->generateDouble() >= p)
            continue;
        // 按概率权重选目标
        double sum = 0.0;
        for (const Transport::Edge &e : n.targets) sum += e.weight;
        double pick = rng->generateDouble() * sum, acc = 0.0;
        int to = -1;
        for (const Transport::Edge &e : n.targets) { acc += e.weight; if (pick <= acc) { to = e.to; break; } }
        if (to < 0) to = n.targets.last().to;
        if (to < 0 || to >= m_facilities.size())
            continue;
        // 目标设施被封锁 -> 本次直接取消发车（不把这部分概率改派给其他目标），
        // 这样去往封闭地的航次彻底消失，出发地的总流量也随之减小。
        const int destRegion = m_facilities[to].region;
        if (n.type == 0 && m_airportClosed.contains(destRegion))
            continue;
        if (n.type == 1 && m_portClosed.contains(destRegion))
            continue;
        const qint64 key = pairKey(n.id, to);
        if (!m_routeGeom.contains(key))
            continue;
        // 载毒概率 = 出发国感染比例 × 传染性 × 海/空途径乘区
        //   出发国感染比例相对“存活人口”（formula 形式二），与封锁/陆地传播口径一致。
        const double infRatio = (srcRegion >= 0 && srcRegion < m_regionInfAlive.size())
                                    ? m_regionInfAlive[srcRegion] : 0.0;
        const double carry = infRatio * m_infectivity * m_routeModifier;
        const bool infected = rng->generateDouble() < carry;
        spawnVehicle(n.type, n.id, to, infected);
        if (m_vehicles.size() >= kMaxVehicles)
            break;
    }
}

// 投放一架飞机/一艘轮船
void MapWidget::spawnVehicle(int type, int from, int to, bool infected)
{
    using namespace GameParams;
    const qint64 key = pairKey(from, to);
    auto git = m_routeGeom.find(key);
    if (git == m_routeGeom.end())
        return;
    const RouteGeom &g = git.value();
    if (g.len <= 0.0 || g.poly.size() < 2)
        return;
    const QPixmap &pm = (type == 0) ? (infected ? m_icoPlaneRed : m_icoPlane)
                                    : (infected ? m_icoShipRed : m_icoShip);
    if (pm.isNull())
        return;

    Vehicle v;
    v.type = type;
    v.from = from;
    v.to = to;
    v.key = key;
    v.infected = infected;
    v.forward = (from < to); // 缓存折线为 低->高 节点，反向则倒着走
    v.t = 0.0;
    const double base = (type == 0) ? kPlaneTravelSec : kShipTravelSec;
    const double maxL = (type == 0) ? m_maxPlaneLen : m_maxShipLen;
    v.dur = qMax(kMinTravelSec, base * (g.len / qMax(1.0, maxL)));

    const qreal lw = pm.width(), lh = pm.height(); // 贴图 dpr=1，像素=逻辑尺寸
    v.item = m_scene->addPixmap(pm);
    v.item->setOffset(-lw / 2.0, -lh / 2.0); // 中心锚定（缩放/旋转绕原点=中心）
    // 场景尺寸固定：随地图缩放等比变化（不再忽略变换）。飞机比轮船大一倍。
    const double sceneSize = (type == 0) ? kPlaneSceneSize : kShipSceneSize;
    const double texMax = qMax(1.0, qMax(lw, lh));
    v.item->setScale(sceneSize / texMax);
    v.item->setZValue(90); // 设施(80) 之上、DNA 气泡(100) 之下
    v.item->setAcceptedMouseButtons(Qt::NoButton);

    // 受感染：现出/加深该航线的红色虚线
    if (infected) { ensureRouteTrail(key); darkenTrail(key); }

    // 初始摆位与朝向
    const double s0 = v.forward ? 0.0 : 1.0;
    double ang = 0.0;
    const QPointF pos = samplePath(g, s0, ang);
    const double heading = v.forward ? ang : ang + 180.0;
    v.item->setPos(pos);
    v.item->setRotation(type == 0 ? heading + 90.0 : heading); // 飞机图朝上(+90)、轮船图朝右(0)
    m_vehicles.push_back(v);
}

// 动画推进：沿固定折线移动；抵达后受感染者感染目标地区
void MapWidget::stepVehicles(double dtSec)
{
    if (m_vehicles.isEmpty())
        return;
    QVector<int> done;
    for (int i = 0; i < m_vehicles.size(); ++i) {
        Vehicle &v = m_vehicles[i];
        auto git = m_routeGeom.find(v.key);
        if (git == m_routeGeom.end()) { done.push_back(i); continue; }
        const RouteGeom &g = git.value();
        v.t += dtSec / qMax(0.001, v.dur);
        if (v.t >= 1.0) { // 抵达
            if (v.infected && v.to >= 0 && v.to < m_facilities.size()) {
                const int destR = m_facilities[v.to].region;
                const int destType = m_facilities[v.to].type;
                // 仅当目标设施未封锁时才感染（formula 形式二：对方设施未封锁）
                const bool destClosed = (destType == 0) ? m_airportClosed.contains(destR)
                                                        : m_portClosed.contains(destR);
                if (destR >= 0 && !destClosed)
                    emit routeInfection(destR);
            }
            done.push_back(i);
            continue;
        }
        const double s = v.forward ? v.t : (1.0 - v.t);
        double ang = 0.0;
        const QPointF pos = samplePath(g, s, ang);
        const double heading = v.forward ? ang : ang + 180.0;
        if (v.item) {
            v.item->setPos(pos);
            v.item->setRotation(v.type == 0 ? heading + 90.0 : heading);
        }
    }
    for (int k = done.size() - 1; k >= 0; --k) {
        const int i = done[k];
        if (m_vehicles[i].item) { m_scene->removeItem(m_vehicles[i].item); delete m_vehicles[i].item; }
        m_vehicles.removeAt(i);
    }
}

// 沿航线（展开坐标）取 s∈[0,1] 处的显示坐标与前进朝向（度）
QPointF MapWidget::samplePath(const RouteGeom &g, double s, double &angleDeg) const
{
    angleDeg = 0.0;
    if (g.poly.isEmpty())
        return QPointF();
    if (g.poly.size() == 1)
        return g.poly.first();
    s = qBound(0.0, s, 1.0);
    const double target = s * g.len;
    int i = 1;
    while (i < g.cum.size() && g.cum[i] < target) ++i;
    if (i >= g.poly.size()) i = g.poly.size() - 1;
    const double segLen = g.cum[i] - g.cum[i - 1];
    const double f = segLen > 1e-9 ? (target - g.cum[i - 1]) / segLen : 0.0;
    const QPointF p0 = g.poly[i - 1], p1 = g.poly[i];
    const QPointF up = p0 + (p1 - p0) * f; // 展开坐标
    const QPointF dir = p1 - p0;
    angleDeg = std::atan2(dir.y(), dir.x()) * 180.0 / kPI;
    // 展开坐标 -> 显示坐标（跨东西缝时取模回绕）
    double x = up.x();
    if (g.wrapped) {
        const double W = m_navRect.width();
        double m = std::fmod(x - m_navRect.left(), W);
        if (m < 0) m += W;
        x = m_navRect.left() + m;
    }
    return QPointF(x, up.y());
}

// 首次出现该航线的红色虚线（跨缝时叠加 ±图宽 的平移副本）
void MapWidget::ensureRouteTrail(qint64 key)
{
    if (m_routeVis.contains(key))
        return;
    auto git = m_routeGeom.find(key);
    if (git == m_routeGeom.end())
        return;
    const RouteGeom &g = git.value();
    QPainterPath path;
    auto addPoly = [&](double dx) {
        if (g.poly.isEmpty()) return;
        bool first = true;
        for (const QPointF &p : g.poly) {
            const QPointF q(p.x() + dx, p.y());
            if (first) { path.moveTo(q); first = false; }
            else path.lineTo(q);
        }
    };
    addPoly(0.0);
    if (g.wrapped) { addPoly(-m_navRect.width()); addPoly(m_navRect.width()); }

    QGraphicsPathItem *item = m_scene->addPath(path);
    item->setZValue(70); // 点阵(50) 之上、设施(80) 之下
    item->setAcceptedMouseButtons(Qt::NoButton);
    item->setBrush(Qt::NoBrush);

    RouteVis vis;
    vis.item = item;
    vis.hits = 0;
    m_routeVis.insert(key, vis);
}

// 受感染载具每通过一次 -> 航线虚线加深一档（浅红 -> 深红）
void MapWidget::darkenTrail(qint64 key)
{
    using namespace GameParams;
    auto it = m_routeVis.find(key);
    if (it == m_routeVis.end())
        return;
    RouteVis &vis = it.value();
    vis.hits = qMin(vis.hits + 1, kTrailMaxHits);
    const double t = static_cast<double>(vis.hits) / static_cast<double>(qMax(1, kTrailMaxHits));
    const int alpha = static_cast<int>(kTrailBaseAlpha + (kTrailMaxAlpha - kTrailBaseAlpha) * t);
    QColor col(220, static_cast<int>(70 * (1.0 - 0.5 * t)), static_cast<int>(70 * (1.0 - 0.5 * t)), alpha);
    QPen pen(col, kTrailWidth);
    pen.setCapStyle(Qt::FlatCap);
    pen.setJoinStyle(Qt::RoundJoin);
    // 飞机航线=实线，轮船航线=虚线（按航线 type 区分）
    int rtype = 1;
    auto git = m_routeGeom.find(key);
    if (git != m_routeGeom.end())
        rtype = git.value().type;
    if (rtype == 1) {
        QVector<qreal> dashes;
        dashes << kTrailDashLen << kTrailGapLen; // 单位=线宽
        pen.setDashPattern(dashes);
    }
    if (vis.item)
        vis.item->setPen(pen);
}

