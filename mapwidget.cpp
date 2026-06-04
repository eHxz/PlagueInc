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
#include <QDebug>
#include <cmath>

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
    m_regionInf[regionId] = inf;
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
    bubble->setOffset(-lw / 2.0, -lh); // 针尖对准地区中心
    bubble->setPos(c);
    bubble->setZValue(100);
    bubble->setToolTip(QString("点击收集 %1 DNA").arg(value));
    m_bubbleValue[bubble] = value;
    // 寿命以“游戏天数”计：暂停时（含进入菜单）不流逝，倍速时更快消失
    m_bubbleLife[bubble] = kBubbleLifeDays;
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
    clearStartBubble();
    m_seeded = false;
    selectRegion(-1);
    m_regionInf.fill(0.0, m_regionInf.size());
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
}

void MapWidget::mousePressEvent(QMouseEvent *event)
{
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

