#include "worldmenu.h"
#include "GameCore.h"

#include <QPainter>
#include <QPainterPath>
#include <QLinearGradient>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QStackedLayout>
#include <QStackedWidget>
#include <QPushButton>
#include <QLabel>
#include <QFrame>
#include <QLocale>
#include <QResizeEvent>
#include <cmath>

static const QColor kHealthyBlue(52, 170, 220);
static const QColor kInfectedRed(190, 30, 55);
static const QColor kDeadBlack(30, 30, 32);

// ============================================================================
//  PieChart
// ============================================================================
PieChart::PieChart(QWidget *parent) : QWidget(parent) {}

void PieChart::setData(double healthy, double infected, double dead)
{
    m_healthy = healthy; m_infected = infected; m_dead = dead;
    update();
}

void PieChart::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    int side = qMin(width(), height()) - 16;
    QRectF box((width() - side) / 2.0, (height() - side) / 2.0, side, side);

    double sum = m_healthy + m_infected + m_dead;
    if (sum <= 0) sum = 1.0;

    struct Slice { double v; QColor c; } slices[3] = {
        {m_healthy, kHealthyBlue}, {m_infected, kInfectedRed}, {m_dead, kDeadBlack}
    };

    p.setPen(QPen(QColor(20, 40, 60), 2));
    double startAngle = 90.0; // 从顶部开始，顺时针
    for (const Slice &s : slices) {
        double span = s.v / sum * 360.0;
        if (span <= 0) continue;
        QRadialGradient g(box.center(), side / 2.0);
        g.setColorAt(0.0, s.c.lighter(125));
        g.setColorAt(1.0, s.c);
        p.setBrush(g);
        p.drawPie(box, static_cast<int>(std::lround(startAngle * 16)),
                  static_cast<int>(std::lround(-span * 16)));
        startAngle -= span;
    }

    // 中心：未受感染占比 + 心形
    int healthyPct = static_cast<int>(std::lround(m_healthy / sum * 100.0));
    p.setPen(Qt::white);
    QFont f = font();
    f.setBold(true);
    f.setPointSize(qMax(14, side / 12));
    p.setFont(f);
    QRectF textBox = box.adjusted(0, side * 0.16, 0, -side * 0.16);
    p.drawText(textBox, Qt::AlignHCenter | Qt::AlignVCenter, QString("%1%").arg(healthyPct));

    // 心形（位于百分比下方）
    double hs = side / 14.0;
    QPointF hc(box.center().x(), box.center().y() + side * 0.20);
    QPainterPath heart;
    heart.moveTo(hc.x(), hc.y() + hs * 0.6);
    heart.cubicTo(hc.x() + hs, hc.y() - hs * 0.3, hc.x() + hs * 0.5, hc.y() - hs, hc.x(), hc.y() - hs * 0.35);
    heart.cubicTo(hc.x() - hs * 0.5, hc.y() - hs, hc.x() - hs, hc.y() - hs * 0.3, hc.x(), hc.y() + hs * 0.6);
    p.setPen(Qt::NoPen);
    p.setBrush(Qt::white);
    p.drawPath(heart);
}

// ============================================================================
//  LineChart
// ============================================================================
LineChart::LineChart(QWidget *parent) : QWidget(parent) {}

void LineChart::setAxis(const QString &yLabel, double yMax, const QString &ySuffix)
{
    m_yLabel = yLabel; m_yMax = yMax > 0 ? yMax : 1.0; m_ySuffix = ySuffix;
    update();
}

void LineChart::setSeries(const QVector<Series> &series, double xMax)
{
    m_series = series; m_xMax = xMax > 0 ? xMax : 1.0;
    update();
}

void LineChart::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    const int left = 150, right = 24, top = 28, bottom = 36;
    QRectF plot(left, top, width() - left - right, height() - top - bottom);
    if (plot.width() < 10 || plot.height() < 10) return;

    // 图例（左侧）
    QFont f = font(); f.setBold(true); f.setPointSize(12); p.setFont(f);
    int ly = 40;
    for (const Series &s : m_series) {
        p.setPen(Qt::NoPen); p.setBrush(s.color);
        p.drawRect(20, ly, 22, 22);
        p.setPen(QColor(235, 235, 240));
        p.drawText(QRectF(50, ly - 2, left - 56, 26), Qt::AlignVCenter | Qt::AlignLeft, s.name);
        ly += 38;
    }

    // 绘图区背景
    p.setPen(Qt::NoPen);
    p.fillRect(plot, QColor(0, 0, 0));

    // 网格 + Y 轴刻度
    p.setPen(QColor(80, 80, 85));
    const int yTicks = 10;
    f.setPointSize(11); p.setFont(f);
    for (int i = 0; i <= yTicks; ++i) {
        double yy = plot.bottom() - plot.height() * i / yTicks;
        p.setPen(QColor(70, 70, 75));
        p.drawLine(QPointF(plot.left(), yy), QPointF(plot.right(), yy));
        double val = m_yMax * i / yTicks;
        p.setPen(QColor(210, 210, 215));
        p.drawText(QRectF(plot.left() - 96, yy - 10, 88, 20),
                   Qt::AlignVCenter | Qt::AlignRight,
                   QString::number(val, 'g', 3) + m_ySuffix);
    }

    // X 轴刻度（时间）
    const int xTicks = 7;
    for (int i = 0; i <= xTicks; ++i) {
        double xx = plot.left() + plot.width() * i / xTicks;
        p.setPen(QColor(70, 70, 75));
        p.drawLine(QPointF(xx, plot.top()), QPointF(xx, plot.bottom()));
        int day = static_cast<int>(std::lround(m_xMax * i / xTicks));
        p.setPen(QColor(210, 210, 215));
        p.drawText(QRectF(xx - 30, plot.bottom() + 4, 60, 18),
                   Qt::AlignHCenter | Qt::AlignTop, QString::number(day));
    }

    // 轴标题
    f.setPointSize(13); p.setFont(f);
    p.setPen(QColor(230, 230, 235));
    p.drawText(QRectF(plot.left(), 2, plot.width(), 22), Qt::AlignHCenter, "时间");

    // 曲线
    for (const Series &s : m_series) {
        if (s.pts.isEmpty()) continue;
        QPainterPath path;
        bool first = true;
        for (const QPointF &pt : s.pts) {
            double xx = plot.left() + plot.width() * qBound(0.0, pt.x() / m_xMax, 1.0);
            double yy = plot.bottom() - plot.height() * qBound(0.0, pt.y() / m_yMax, 1.0);
            if (first) { path.moveTo(xx, yy); first = false; }
            else path.lineTo(xx, yy);
        }
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(s.color, 2.4));
        p.drawPath(path);
    }
}

// ============================================================================
//  ReportWidget
// ============================================================================
ReportWidget::ReportWidget(QWidget *parent) : QWidget(parent) {}

void ReportWidget::setLists(const QStringList &healthy, const QStringList &infected, const QStringList &dead)
{
    m_healthy = healthy; m_infected = infected; m_dead = dead;
    update();
}

void ReportWidget::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    struct Col { QString icon; QColor color; QString title; const QStringList *list; };
    const Col cols[3] = {
        {"♥", kHealthyBlue, "未受感染的国家", &m_healthy},
        {"☣", kInfectedRed, "受感染的国家", &m_infected},
        {"☠", QColor(120, 120, 125), "被摧毁国家", &m_dead},
    };

    int colW = width() / 3;
    QFont f = font(); f.setBold(true);
    for (int c = 0; c < 3; ++c) {
        int x = c * colW + 16;
        // 标题
        f.setPointSize(15); p.setFont(f);
        p.setPen(cols[c].color);
        p.drawText(QRectF(x, 12, 36, 30), Qt::AlignVCenter | Qt::AlignLeft, cols[c].icon);
        p.setPen(QColor(235, 235, 240));
        p.drawText(QRectF(x + 38, 12, colW - 60, 30), Qt::AlignVCenter | Qt::AlignLeft, cols[c].title);

        // 列表
        f.setPointSize(13); p.setFont(f);
        int y = 56;
        for (const QString &name : *cols[c].list) {
            if (y > height() - 24) break;
            p.setPen(QColor(220, 220, 225));
            p.drawText(QRectF(x, y, colW - 32, 26), Qt::AlignVCenter | Qt::AlignLeft, name);
            y += 30;
        }
    }
}

// ============================================================================
//  WorldMenu
// ============================================================================
WorldMenu::WorldMenu(GameCore *core, QWidget *parent)
    : QWidget(parent), m_core(core)
{
    setAttribute(Qt::WA_NoSystemBackground);

    QVBoxLayout *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // ---- 顶部分页栏 ----
    QFrame *tabBar = new QFrame();
    tabBar->setObjectName("WMenuTabBar");
    tabBar->setFixedHeight(56);
    QHBoxLayout *tabLayout = new QHBoxLayout(tabBar);
    tabLayout->setContentsMargins(0, 0, 0, 0);
    tabLayout->setSpacing(0);
    const QStringList tabNames = {"世界概况", "其他数据"};
    for (int i = 0; i < tabNames.size(); ++i) {
        QPushButton *b = new QPushButton(tabNames[i]);
        b->setObjectName("WTabBtn");
        b->setCheckable(true);
        b->setCursor(Qt::PointingHandCursor);
        connect(b, &QPushButton::clicked, this, [this, i]() { onTabClicked(i); });
        m_tabButtons.append(b);
        tabLayout->addWidget(b, 1);
    }
    QPushButton *closeBtn = new QPushButton("✖");
    closeBtn->setObjectName("WCloseBtn");
    closeBtn->setFixedWidth(64);
    closeBtn->setCursor(Qt::PointingHandCursor);
    connect(closeBtn, &QPushButton::clicked, this, [this]() { emit closed(); });
    tabLayout->addWidget(closeBtn);
    root->addWidget(tabBar);

    // ---- 内容区 ----
    QWidget *contentHost = new QWidget();
    contentHost->setAttribute(Qt::WA_NoSystemBackground);
    m_content = new QStackedLayout(contentHost);
    m_content->setContentsMargins(0, 0, 0, 0);
    root->addWidget(contentHost, 1);

    buildOverviewPage();
    buildDataPage();

    connect(m_core, &GameCore::cureProgressChanged, this, [this](double) { if (isVisible()) refreshAll(); });
    connect(m_core, &GameCore::globalStatsUpdated, this, [this](long long, long long, long long, long long) {
        if (isVisible()) refreshAll();
    });

    setStyleSheet(R"(
        #WMenuTabBar { background-color: rgba(228,232,236,238); }
        #WTabBtn {
            background: transparent; color: #355; font-weight: bold;
            font-size: 17px; border: none;
        }
        #WTabBtn:hover { background: rgba(255,255,255,200); }
        #WTabBtn:checked { background: #0093d6; color: white; }
        #WCloseBtn {
            color: #0093d6; font-size: 22px; font-weight: 900;
            background: transparent; border: none;
        }
        #WCloseBtn:hover { background: rgba(255,255,255,160); }
        #CurePanel {
            background-color: rgba(8,10,14,235);
            border: 1px solid rgba(40,90,130,200); border-radius: 6px;
        }
        #CureTitle { color: #ffffff; font-size: 22px; font-weight: bold; }
        #CureSub { color: #cfe6f5; font-size: 16px; }
        #CureDays { color: #2fd0ff; font-size: 26px; font-weight: bold; }
        #FlaskBox { background-color: rgba(200,200,205,235); border-radius: 6px; }
        #CurePercent { color: #111; font-size: 48px; font-weight: bold; }
        #StatLine { color: #eaf4ff; font-size: 22px; font-weight: bold; }
        #DataRow {
            background-color: rgba(10,14,20,180); color: #ffffff;
            font-size: 20px; font-weight: bold; text-align: left;
            padding: 18px 28px; border: none; border-bottom: 1px solid rgba(60,90,120,140);
        }
        #DataRow:hover { background-color: rgba(20,40,60,210); }
        #ChartHeader { background-color: rgba(10,80,130,235); }
        #ChartTitle { color: #ffffff; font-size: 20px; font-weight: bold; }
        #BackBtn { color: #ffffff; font-size: 22px; font-weight: 900; background: transparent; border: none; }
        #BackBtn:hover { background: rgba(255,255,255,60); }
    )");
}

void WorldMenu::buildOverviewPage()
{
    QWidget *page = new QWidget();
    page->setAttribute(Qt::WA_NoSystemBackground);
    QHBoxLayout *h = new QHBoxLayout(page);
    h->setContentsMargins(40, 30, 40, 40);
    h->setSpacing(30);

    // 左：扇形图 + 三项统计
    QWidget *leftBox = new QWidget();
    leftBox->setAttribute(Qt::WA_NoSystemBackground);
    QVBoxLayout *lv = new QVBoxLayout(leftBox);
    lv->setContentsMargins(0, 0, 0, 0);
    m_pie = new PieChart();
    lv->addWidget(m_pie, 1);

    m_statHealthy = new QLabel();  m_statHealthy->setObjectName("StatLine");
    m_statInfected = new QLabel(); m_statInfected->setObjectName("StatLine");
    m_statDead = new QLabel();     m_statDead->setObjectName("StatLine");
    lv->addSpacing(8);
    lv->addWidget(m_statHealthy);
    lv->addWidget(m_statInfected);
    lv->addWidget(m_statDead);
    h->addWidget(leftBox, 1);

    // 右：解药研发面板
    QFrame *cure = new QFrame();
    cure->setObjectName("CurePanel");
    QVBoxLayout *cv = new QVBoxLayout(cure);
    cv->setContentsMargins(28, 28, 28, 28);
    cv->setSpacing(14);
    m_cureTitle = new QLabel();   m_cureTitle->setObjectName("CureTitle");   m_cureTitle->setWordWrap(true);
    m_cureSub = new QLabel();     m_cureSub->setObjectName("CureSub");       m_cureSub->setWordWrap(true);
    m_cureDays = new QLabel();    m_cureDays->setObjectName("CureDays");
    cv->addWidget(m_cureTitle);
    cv->addStretch();
    cv->addWidget(m_cureSub);
    cv->addWidget(m_cureDays);
    cv->addSpacing(10);

    QFrame *flask = new QFrame();
    flask->setObjectName("FlaskBox");
    flask->setMinimumHeight(120);
    QHBoxLayout *fl = new QHBoxLayout(flask);
    fl->setContentsMargins(24, 12, 24, 12);
    QLabel *flaskIcon = new QLabel("\xF0\x9F\xA7\xAA"); // 🧪
    flaskIcon->setStyleSheet("font-size:56px;");
    m_curePercent = new QLabel();
    m_curePercent->setObjectName("CurePercent");
    m_curePercent->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    fl->addWidget(flaskIcon);
    fl->addStretch();
    fl->addWidget(m_curePercent);
    cv->addWidget(flask);

    h->addWidget(cure, 1);

    m_content->addWidget(page); // index 0
}

// 包一层带标题/返回按钮的图表页
static QWidget *wrapChart(const QString &title, QWidget *chart, QStackedWidget *stack)
{
    QWidget *page = new QWidget();
    QVBoxLayout *v = new QVBoxLayout(page);
    v->setContentsMargins(0, 0, 0, 0);
    v->setSpacing(0);
    QFrame *hdr = new QFrame();
    hdr->setObjectName("ChartHeader");
    hdr->setFixedHeight(50);
    QHBoxLayout *hl = new QHBoxLayout(hdr);
    QLabel *t = new QLabel(title);
    t->setObjectName("ChartTitle");
    QPushButton *back = new QPushButton("✖");
    back->setObjectName("BackBtn");
    back->setFixedWidth(56);
    back->setCursor(Qt::PointingHandCursor);
    QObject::connect(back, &QPushButton::clicked, stack, [stack]() { stack->setCurrentIndex(0); });
    hl->addWidget(t, 1);
    hl->addWidget(back);
    v->addWidget(hdr);
    v->addWidget(chart, 1);
    return page;
}

void WorldMenu::buildDataPage()
{
    QWidget *page = new QWidget();
    page->setAttribute(Qt::WA_NoSystemBackground);
    QVBoxLayout *pv = new QVBoxLayout(page);
    pv->setContentsMargins(0, 0, 0, 0);

    m_dataStack = new QStackedWidget();
    m_dataStack->setAttribute(Qt::WA_NoSystemBackground);
    pv->addWidget(m_dataStack);

    // index 0：四个入口
    QWidget *list = new QWidget();
    list->setAttribute(Qt::WA_NoSystemBackground);
    QVBoxLayout *lv = new QVBoxLayout(list);
    lv->setContentsMargins(0, 0, 0, 0);
    lv->setSpacing(0);
    const QStringList rows = {"传染病疫情报告", "全球人口曲线图", "解药研制曲线图", "疾病进化曲线图"};
    for (int i = 0; i < rows.size(); ++i) {
        QPushButton *b = new QPushButton("  " + rows[i] + "                    ›");
        b->setObjectName("DataRow");
        b->setCursor(Qt::PointingHandCursor);
        connect(b, &QPushButton::clicked, this, [this, i]() { showChart(i + 1); });
        lv->addWidget(b);
    }
    lv->addStretch();
    m_dataStack->addWidget(list); // 0

    m_report = new ReportWidget();
    m_popChart = new LineChart();
    m_cureChart = new LineChart();
    m_evoChart = new LineChart();
    m_dataStack->addWidget(wrapChart("传染病疫情报告", m_report, m_dataStack));  // 1
    m_dataStack->addWidget(wrapChart("全球人口曲线图", m_popChart, m_dataStack)); // 2
    m_dataStack->addWidget(wrapChart("解药研制曲线图", m_cureChart, m_dataStack)); // 3
    m_dataStack->addWidget(wrapChart("疾病进化曲线图", m_evoChart, m_dataStack));  // 4

    m_content->addWidget(page); // index 1
}

void WorldMenu::showChart(int which)
{
    refreshAll();
    m_dataStack->setCurrentIndex(which);
}

void WorldMenu::openMenu()
{
    onTabClicked(0);
    show();
    raise();
}

void WorldMenu::onTabClicked(int tab)
{
    m_tab = tab;
    for (int i = 0; i < m_tabButtons.size(); ++i)
        m_tabButtons[i]->setChecked(i == tab);
    m_content->setCurrentIndex(tab);
    if (tab == 1 && m_dataStack)
        m_dataStack->setCurrentIndex(0);
    refreshAll();
}

void WorldMenu::refreshAll()
{
    const long long total = m_core->worldPopulation();
    const long long infected = m_core->worldInfected();
    const long long dead = m_core->worldDead();
    const long long healthy = qMax(0LL, total - infected - dead);

    double hf = total > 0 ? double(healthy) / total : 1.0;
    double inf = total > 0 ? double(infected) / total : 0.0;
    double df = total > 0 ? double(dead) / total : 0.0;

    // 概况页
    if (m_pie) m_pie->setData(hf, inf, df);
    QLocale loc(QLocale::English);
    if (m_statHealthy) m_statHealthy->setText(QString("♥  %1").arg(loc.toString(healthy)));
    if (m_statInfected) m_statInfected->setText(QString("☣  %1").arg(loc.toString(infected)));
    if (m_statDead) m_statDead->setText(QString("☠  %1").arg(loc.toString(dead)));

    const QString name = m_core->diseaseName();
    const bool disc = m_core->isDiscovered();
    const double cure = m_core->cureProgress();
    if (m_cureTitle) m_cureTitle->setText(disc ? QString("%1 已被各国发现").arg(name)
                                               : QString("%1 尚未被发现").arg(name));
    if (m_cureSub) m_cureSub->setText(QString("距 %1 的解药研发完成还需").arg(name));
    if (m_cureDays) {
        QString days;
        double rate = m_core->cureRatePerDay();
        if (cure >= 100.0) days = "0 天";
        else if (!disc || rate <= 1e-9) days = "— 天";
        else days = QString("%1 天").arg(static_cast<int>(std::ceil((100.0 - cure) / rate)));
        m_cureDays->setText(days);
    }
    if (m_curePercent) m_curePercent->setText(QString("%1%").arg(static_cast<int>(cure)));

    // 数据页：报告
    if (m_report) {
        QStringList lh, li, ld;
        for (const RegionData &r : m_core->regions()) {
            if (r.alive() <= 0) ld << r.name;
            else if (r.isInfected && r.infectedCount > 0) li << r.name;
            else lh << r.name;
        }
        m_report->setLists(lh, li, ld);
    }

    // 数据页：曲线
    const QVector<HistoryPoint> &hist = m_core->history();
    double xMax = qMax(1, m_core->currentDay());

    if (m_popChart) {
        LineChart::Series sH{"未受感染", kHealthyBlue, {}}, sI{"受感染", kInfectedRed, {}}, sD{"死亡", kDeadBlack.lighter(160), {}};
        for (const HistoryPoint &p : hist) {
            sH.pts << QPointF(p.day, p.healthyFrac * 100.0);
            sI.pts << QPointF(p.day, p.infectedFrac * 100.0);
            sD.pts << QPointF(p.day, p.deadFrac * 100.0);
        }
        m_popChart->setAxis("全球人口", 100.0, "%");
        m_popChart->setSeries({sH, sI, sD}, xMax);
    }
    if (m_cureChart) {
        LineChart::Series sDem{"解药研发需求", kInfectedRed, {}}, sRes{"已进行的研究", kHealthyBlue, {}};
        for (const HistoryPoint &p : hist) {
            sDem.pts << QPointF(p.day, 100.0);
            sRes.pts << QPointF(p.day, p.cure);
        }
        m_cureChart->setAxis("科研预算", 100.0, "%");
        m_cureChart->setSeries({sDem, sRes}, xMax);
    }
    if (m_evoChart) {
        LineChart::Series sInf{"传染性", QColor(231, 41, 142), {}},
                          sSev{"严重性", QColor(210, 220, 40), {}},
                          sLet{"致命性", QColor(150, 60, 220), {}};
        double yMax = 1.0;
        for (const HistoryPoint &p : hist) {
            sInf.pts << QPointF(p.day, p.infectivity);
            sSev.pts << QPointF(p.day, p.severity);
            sLet.pts << QPointF(p.day, p.lethality);
            yMax = qMax(yMax, qMax(p.infectivity, qMax(p.severity, p.lethality)));
        }
        m_evoChart->setAxis("疾病点数", std::ceil(yMax * 10) / 10.0, "");
        m_evoChart->setSeries({sInf, sSev, sLet}, xMax);
    }
}

void WorldMenu::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    QLinearGradient g(0, 0, 0, height());
    g.setColorAt(0.0, QColor(10, 40, 70, 150));
    g.setColorAt(1.0, QColor(20, 70, 120, 185));
    p.fillRect(rect(), g);
}
