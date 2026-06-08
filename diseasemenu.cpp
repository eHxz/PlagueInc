#include "diseasemenu.h"
#include "hexnode.h"
#include "GameCore.h"

#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QLinearGradient>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QFrame>
#include <QDate>
#include <QResizeEvent>
#include <QHideEvent>
#include <QMediaPlayer>
#include <QVideoSink>
#include <QCoreApplication>
#include <QUrl>
#include <cmath>

// ============================================================================
//  HexGrid：平顶六边形蜂窝骨架（细线点阵）+ 格坐标 / 吸附
//    几何（边长 R）：宽 = 15.5R（10 列，列距 1.5R），高 = 6√3R（对边距离 √3R）。
//    偶数列 6 格、奇数列 5 格，合计 55 格；只画落在矩形内部的完整六边形。
// ============================================================================
namespace {
const double kSqrt3 = 1.7320508075688772;
const double kHcWidthInR  = 15.5;          // 蜂窝矩形宽 / R
const double kHcHeightInR = 6.0 * kSqrt3;  // 蜂窝矩形高 / R
const double kGridMargin  = 14.0;          // 蜂窝四周留白
}

HexGrid::HexGrid(QWidget *parent)
    : QWidget(parent)
{
    setAttribute(Qt::WA_NoSystemBackground); // 透出菜单红色渐变
}

void HexGrid::recompute()
{
    const double availW = width() - m_rightInset - 2.0 * kGridMargin;
    const double availH = height() - 2.0 * kGridMargin;
    if (availW < 2.0 || availH < 2.0) {
        m_R = 1.0;
        m_origin = QPointF(kGridMargin, kGridMargin);
        return;
    }
    m_R = qMax(1.0, std::min(availW / kHcWidthInR, availH / kHcHeightInR));
    const double hcW = kHcWidthInR * m_R;
    const double hcH = kHcHeightInR * m_R;
    // 在左侧（扣除右侧面板预留后）可用区里居中
    m_origin = QPointF(kGridMargin + (availW - hcW) / 2.0,
                       kGridMargin + (availH - hcH) / 2.0);
}

QSizeF HexGrid::cellSize() const
{
    return QSizeF(2.0 * m_R, kSqrt3 * m_R);
}

QPointF HexGrid::cellCenter(int col, int row) const
{
    const double cx = m_origin.x() + m_R + 1.5 * m_R * col;
    const double cy = (col % 2 == 0)
        ? m_origin.y() + kSqrt3 * m_R / 2.0 + kSqrt3 * m_R * row   // 偶数列：首格顶边贴矩形上沿
        : m_origin.y() + kSqrt3 * m_R + kSqrt3 * m_R * row;        // 奇数列：下移半格
    return QPointF(cx, cy);
}

// 编号 1..55 -> (列,行)：一列一列、自上而下。偶数列 6 格、奇数列 5 格。
bool HexGrid::indexToColRow(int n, int &col, int &row)
{
    int idx = 1;
    for (int c = 0; c < kCols; ++c) {
        const int rows = (c % 2 == 0) ? kRowsEven : kRowsOdd;
        if (n >= idx && n < idx + rows) {
            col = c;
            row = n - idx;
            return true;
        }
        idx += rows;
    }
    return false;
}

QPointF HexGrid::cellCenterByIndex(int n) const
{
    int col = 0, row = 0;
    if (!indexToColRow(n, col, row))
        return m_origin;
    return cellCenter(col, row);
}

QPointF HexGrid::snapNormalized(const QPointF &norm, QSet<int> &occupied,
                                int &outCol, int &outRow) const
{
    const double hcW = kHcWidthInR * m_R;
    const double hcH = kHcHeightInR * m_R;
    const QPointF target(m_origin.x() + norm.x() * hcW,
                         m_origin.y() + norm.y() * hcH);
    double best = 1e18;
    int bc = 0, br = 0;
    QPointF bp = target;
    for (int col = 0; col < kCols; ++col) {
        const int rows = rowsInColumn(col);
        for (int row = 0; row < rows; ++row) {
            if (occupied.contains(col * 100 + row))
                continue;
            const QPointF c = cellCenter(col, row);
            const double dx = c.x() - target.x();
            const double dy = c.y() - target.y();
            const double d = dx * dx + dy * dy;
            if (d < best) { best = d; bc = col; br = row; bp = c; }
        }
    }
    occupied.insert(bc * 100 + br);
    outCol = bc;
    outRow = br;
    return bp;
}

void HexGrid::setActive(bool a)
{
    if (m_active == a)
        return;
    m_active = a;
    update();
}

void HexGrid::setRightInset(int px)
{
    // 总是重算：即使预留宽度不变，窗口尺寸也可能已变化（layoutHexes 每次调用前刷新）
    m_rightInset = px;
    recompute();
    update();
}

void HexGrid::resizeEvent(QResizeEvent *)
{
    recompute();
}

void HexGrid::paintEvent(QPaintEvent *)
{
    if (!m_active)
        return;
    recompute();

    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    // 所有格子拼成一条路径再一次性描边：共享的格壁只画一遍，避免叠加变亮的接缝。
    QPainterPath path;
    for (int col = 0; col < kCols; ++col) {
        const int rows = rowsInColumn(col);
        for (int row = 0; row < rows; ++row) {
            const QPointF c = cellCenter(col, row);
            QPolygonF hex;
            for (int i = 0; i < 6; ++i) {
                const double ang = M_PI / 180.0 * (60.0 * i); // 平顶
                hex << QPointF(c.x() + m_R * std::cos(ang),
                               c.y() + m_R * std::sin(ang));
            }
            path.addPolygon(hex);
            path.closeSubpath();
        }
    }
    p.strokePath(path, QPen(QColor(235, 175, 185, 70), 1.0));
}

// ============================================================================
//  StatBars：底部 DNA + 传染性 / 严重性 / 致命性
// ============================================================================
StatBars::StatBars(QWidget *parent)
    : QWidget(parent)
{
    setMinimumHeight(72);
}

void StatBars::setDNA(int dna)
{
    m_dna = dna;
    update();
}

void StatBars::setStats(const DiseaseStats &s)
{
    m_stats = s;
    update();
}

void StatBars::setPreview(const DiseaseStats &delta)
{
    m_preview = delta;
    update();
}

void StatBars::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    // 背景
    p.fillRect(rect(), QColor(8, 8, 10, 245));

    QFont f = font();
    f.setBold(true);

    const int W = width();
    const int H = height();

    // 左侧 DNA
    int dnaW = qMin(220, W / 4);
    QRectF dnaBox(12, 10, dnaW - 24, H - 20);
    p.setBrush(QColor(35, 35, 40, 230));
    p.setPen(QPen(QColor(90, 90, 100), 1));
    p.drawRoundedRect(dnaBox, 4, 4);
    f.setPointSize(15);
    p.setFont(f);
    p.setPen(QColor(230, 230, 235));
    p.drawText(dnaBox.adjusted(12, 0, -12, 0), Qt::AlignVCenter | Qt::AlignLeft, "DNA");
    p.drawText(dnaBox.adjusted(12, 0, -12, 0), Qt::AlignVCenter | Qt::AlignRight,
               m_dna >= 999999 ? QStringLiteral("∞") : QString::number(m_dna));

    // 右侧三个属性条
    struct Item { QString name; double value; double delta; double maxv; QColor color; };
    const Item items[3] = {
        {"传染性", m_stats.infectivity, m_preview.infectivity, 1.0, QColor(231, 41, 142)},
        {"严重性", m_stats.severity, m_preview.severity, 1.0, QColor(238, 205, 40)},
        {"致命性", m_stats.lethality, m_preview.lethality, 0.6, QColor(120, 90, 220)},
    };

    int areaX = dnaW + 8;
    int areaW = W - areaX - 16;
    int colW = areaW / 3;
    for (int i = 0; i < 3; ++i) {
        int x = areaX + i * colW;
        QRectF label(x, 8, colW - 16, 22);
        f.setPointSize(12);
        p.setFont(f);
        p.setPen(QColor(225, 225, 230));
        p.drawText(label, Qt::AlignVCenter | Qt::AlignLeft, items[i].name);

        QRectF bar(x, 38, colW - 24, 16);
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(45, 45, 50));
        p.drawRoundedRect(bar, 3, 3);

        double frac = qBound(0.0, items[i].value / items[i].maxv, 1.0);
        double fracTotal = qBound(0.0, (items[i].value + items[i].delta) / items[i].maxv, 1.0);

        // 浅色段：选中技能后将增加的部分（深色当前值之后）
        if (fracTotal > frac) {
            QRectF pre(bar.left() + bar.width() * frac, bar.top(),
                       bar.width() * (fracTotal - frac), bar.height());
            p.setBrush(items[i].color.lighter(175));
            p.drawRoundedRect(pre, 3, 3);
        }
        // 深色段：当前值
        if (frac > 0.0) {
            QRectF fill(bar.left(), bar.top(), bar.width() * frac, bar.height());
            p.setBrush(items[i].color);
            p.drawRoundedRect(fill, 3, 3);
        }
    }
}

// ============================================================================
//  VideoBackground：概况(首页)循环播放的背景视频
// ============================================================================
VideoBackground::VideoBackground(QWidget *parent)
    : QWidget(parent)
{
    setAttribute(Qt::WA_NoSystemBackground);
    setAttribute(Qt::WA_TransparentForMouseEvents); // 背景层不拦截点击

    m_player = new QMediaPlayer(this);
    m_sink = new QVideoSink(this);
    m_player->setVideoSink(m_sink);
    m_player->setLoops(QMediaPlayer::Infinite); // 重复播放（无限循环）
    // 不挂 QAudioOutput：视频静音，避免干扰背景音乐。

    connect(m_sink, &QVideoSink::videoFrameChanged, this,
            [this](const QVideoFrame &f) { m_frame = f; update(); });
}

void VideoBackground::setSource(const QString &filePath)
{
    m_player->setSource(QUrl::fromLocalFile(filePath));
}

void VideoBackground::start()
{
    if (m_player->playbackState() != QMediaPlayer::PlayingState)
        m_player->play();
}

void VideoBackground::stop()
{
    m_player->pause();
}

void VideoBackground::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    if (m_frame.isValid()) {
        QVideoFrame::PaintOptions opts;
        opts.aspectRatioMode = Qt::KeepAspectRatioByExpanding; // 铺满并裁切
        m_frame.paint(&p, rect(), opts);
    } else {
        p.fillRect(rect(), QColor(20, 8, 12)); // 首帧到来前的底色
    }
    // 轻微暗红叠加：让视频融入菜单红色调，并保证上层概况文字可读
    p.fillRect(rect(), QColor(80, 12, 24, 90));
}

// ============================================================================
//  DiseaseMenu
// ============================================================================
DiseaseMenu::DiseaseMenu(GameCore *core, QWidget *parent)
    : QWidget(parent)
    , m_core(core)
{
    setAttribute(Qt::WA_NoSystemBackground); // 让地图透出来

    QVBoxLayout *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // ---- 顶部分页栏 ----
    QFrame *tabBar = new QFrame();
    tabBar->setObjectName("MenuTabBar");
    tabBar->setFixedHeight(56);
    QHBoxLayout *tabLayout = new QHBoxLayout(tabBar);
    tabLayout->setContentsMargins(0, 0, 0, 0);
    tabLayout->setSpacing(0);

    const QStringList tabNames = {"疾病概况", "传播途径", "发病症状", "特殊能力"};
    for (int i = 0; i < tabNames.size(); ++i) {
        QPushButton *b = new QPushButton(tabNames[i]);
        b->setObjectName("TabBtn");
        b->setCheckable(true);
        b->setCursor(Qt::PointingHandCursor);
        connect(b, &QPushButton::clicked, this, [this, i]() { onTabClicked(i); });
        m_tabButtons.append(b);
        tabLayout->addWidget(b, 1);
    }
    QPushButton *closeBtn = new QPushButton("✖");
    closeBtn->setObjectName("CloseBtn");
    closeBtn->setFixedWidth(64);
    closeBtn->setCursor(Qt::PointingHandCursor);
    connect(closeBtn, &QPushButton::clicked, this, [this]() { emit closed(); });
    tabLayout->addWidget(closeBtn);

    root->addWidget(tabBar);

    // ---- 中部：技能区（透明蜂窝骨架，承载六边形 + 信息面板 + 概况面板）----
    m_hexArea = new HexGrid();
    root->addWidget(m_hexArea, 1);

    // 信息面板（右侧，绝对定位于 hexArea 上）
    m_infoPanel = new QFrame(m_hexArea);
    m_infoPanel->setObjectName("InfoPanel");
    QVBoxLayout *ip = new QVBoxLayout(m_infoPanel);
    ip->setContentsMargins(16, 16, 16, 16);
    m_infoTitle = new QLabel();
    m_infoTitle->setObjectName("InfoTitle");
    m_infoTitle->setWordWrap(true);
    m_infoBody = new QLabel();
    m_infoBody->setObjectName("InfoBody");
    m_infoBody->setWordWrap(true);
    m_infoBody->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    m_evolveBtn = new QPushButton("进化");
    m_evolveBtn->setObjectName("EvolveBtn");
    m_evolveBtn->setCursor(Qt::PointingHandCursor);
    connect(m_evolveBtn, &QPushButton::clicked, this, &DiseaseMenu::onEvolveClicked);
    ip->addWidget(m_infoTitle);
    ip->addSpacing(8);
    ip->addWidget(m_infoBody, 1);
    ip->addWidget(m_evolveBtn);

    // 概况面板（左上）
    m_overview = new QLabel(m_hexArea);
    m_overview->setObjectName("OverviewPanel");
    m_overview->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    m_overview->setWordWrap(true);

    // 概况(首页)背景循环视频：随可执行文件部署在 media/pathogen.mp4（见 CMake POST_BUILD）。
    // 置于最底层（概况面板之下、菜单红色渐变之上），仅在概况页可见并播放。
    m_videoBg = new VideoBackground(m_hexArea);
    m_videoBg->setSource(QCoreApplication::applicationDirPath() + "/media/pathogen.mp4");
    m_videoBg->lower();
    m_videoBg->hide();

    // ---- 底部状态条 ----
    m_statBars = new StatBars();
    root->addWidget(m_statBars);

    // ---- 连接核心 ----
    connect(m_core, &GameCore::skillsChanged, this, &DiseaseMenu::rebuildSkills);
    connect(m_core, &GameCore::skillsChanged, this, &DiseaseMenu::updateInfoPanel);
    connect(m_core, &GameCore::dnaPointsChanged, this, &DiseaseMenu::refreshDNA);
    connect(m_core, &GameCore::diseaseStatsChanged, this, &DiseaseMenu::refreshStats);

    setStyleSheet(R"(
        #MenuTabBar { background-color: rgba(228,228,230,238); }
        #TabBtn {
            background: transparent; color: #555; font-weight: bold;
            font-size: 17px; border: none;
        }
        #TabBtn:hover { background: rgba(255,255,255,200); }
        #TabBtn:checked { background: #b01020; color: white; }
        #CloseBtn {
            color: #b01020; font-size: 22px; font-weight: 900;
            background: transparent; border: none;
        }
        #CloseBtn:hover { background: rgba(255,255,255,160); }
        #InfoPanel {
            background-color: rgba(16,11,13,238);
            border: 1px solid rgba(130,45,55,200); border-radius: 6px;
        }
        #InfoTitle { color: #ffffff; font-size: 18px; font-weight: bold; }
        #InfoBody { color: #e8c9cd; font-size: 14px; }
        #EvolveBtn {
            background-color: rgba(70,70,72,235); color: #ff5a6a;
            font-size: 18px; font-weight: bold; border: none;
            border-radius: 4px; padding: 12px;
        }
        #EvolveBtn:hover { background-color: rgba(90,90,92,235); }
        #EvolveBtn:disabled { color: #888; background-color: rgba(55,55,57,235); }
        #OverviewPanel {
            background-color: rgba(10,7,8,215); color: #e6c6ca;
            font-size: 15px; padding: 16px;
            border: 1px solid rgba(130,45,55,170);
        }
    )");
}

int DiseaseMenu::skillPageForTab(int tab) const
{
    switch (tab) {
    case 1: return SkillTransmission;
    case 2: return SkillSymptom;
    case 3: return SkillAbility;
    default: return -1;
    }
}

void DiseaseMenu::openMenu()
{
    openAtTab(0);
}

void DiseaseMenu::openAtTab(int tab)
{
    onTabClicked(tab);
    refreshStats();
    refreshDNA();
    show();
    raise();
}

void DiseaseMenu::onTabClicked(int tab)
{
    m_tab = tab;
    m_selectedSkill.clear();
    for (int i = 0; i < m_tabButtons.size(); ++i)
        m_tabButtons[i]->setChecked(i == tab);

    const bool overview = (tab == 0);
    m_overview->setVisible(overview);
    m_infoPanel->setVisible(!overview);

    // 背景视频仅在概况(首页)循环播放；切到其它分页则暂停省电。
    if (m_videoBg) {
        m_videoBg->setVisible(overview);
        if (overview) {
            m_videoBg->lower();   // 始终压在概况面板之下
            m_videoBg->start();
        } else {
            m_videoBg->stop();
        }
    }

    if (overview) {
        QDate startDate = m_core->startDate();
        DiseaseStats d = m_core->diseaseStats();
        QString origin = "未知";
        for (const RegionData &r : m_core->regions())
            if (r.isInfected) { origin = r.name; break; }
        m_overview->setText(
            QString("<b style='font-size:22px;color:#ff8088;'>%1</b><br>"
                    "<span style='color:#c89aa0;'>疾病概况</span><br><br>"
                    "开始地点：%2<br>"
                    "开始日期：%3<br><br>"
                    "传染性：%4<br>"
                    "严重性：%5<br>"
                    "致命性：%6<br>"
                    "解药状态：%7<br><br>"
                    "提示：在地图上点击一个地区即可选择疫情起源。")
                .arg(m_core->diseaseName())
                .arg(origin)
                .arg(startDate.toString("dd MMMM yyyy"))
                .arg(QString::number(d.infectivity, 'f', 2))
                .arg(QString::number(d.severity, 'f', 2))
                .arg(QString::number(d.lethality, 'f', 2))
                .arg(m_core->isDiscovered() ? "已被发现，疫苗研发中" : "尚未被发现"));
    }

    rebuildSkills();
    updateInfoPanel();
    layoutHexes();
}

void DiseaseMenu::rebuildSkills()
{
    qDeleteAll(m_hexes);
    m_hexes.clear();

    int page = skillPageForTab(m_tab);
    if (page < 0)
        return;

    const QVector<SkillDef> &defs = m_core->skills(page);
    for (const SkillDef &s : defs) {
        if (!m_core->isRevealed(s.id))
            continue;
        HexNode *h = new HexNode(s.id, m_hexArea);
        h->setGlyph(s.glyph);
        // 六边形技能图标：已在打包时裁掉透明边并顺时针旋转 90°（平顶六边形），直接载入即可。
        QPixmap raw(QString(":/hex/%1/%2.png").arg(page).arg(s.cell));
        if (!raw.isNull())
            h->setPixmap(raw);
        h->setState(m_core->isUnlocked(s.id), m_core->isBuyable(s.id));
        h->setSelected(s.id == m_selectedSkill);
        connect(h, &HexNode::clicked, this, &DiseaseMenu::onHexClicked);
        h->show();
        m_hexes.append(h);
    }
    layoutHexes();
}

void DiseaseMenu::layoutHexes()
{
    if (!m_hexArea)
        return;
    const int aw = m_hexArea->width();
    const int ah = m_hexArea->height();

    // 背景视频：铺满整个技能区
    if (m_videoBg)
        m_videoBg->setGeometry(0, 0, aw, ah);

    // 信息面板：右侧
    const int panelW = qMin(320, aw / 3);
    m_infoPanel->setGeometry(aw - panelW - 16, 14, panelW, ah - 28);
    // 概况面板：左上
    m_overview->setGeometry(16, 14, qMin(360, aw / 2), qMin(300, ah - 28));

    const int page = skillPageForTab(m_tab);
    // 蜂窝骨架仅在三个技能页显示；预留右侧信息面板区，蜂窝在其左侧居中。
    m_hexArea->setActive(page >= 0);
    m_hexArea->setRightInset(m_infoPanel->isVisible() ? panelW + 32 : 0);
    if (page < 0)
        return;
    const QVector<SkillDef> &defs = m_core->skills(page);

    // 每个已显形的技能：直接落在它指定的蜂窝格（cell 1..55）中心。
    const QSizeF cs = m_hexArea->cellSize();
    const int wPad = 8; // 选中描边余量
    const int wW = static_cast<int>(cs.width())  + wPad;
    const int wH = static_cast<int>(cs.height()) + wPad;
    for (HexNode *h : m_hexes) {
        const SkillDef *def = nullptr;
        for (const SkillDef &s : defs)
            if (s.id == h->skillId()) { def = &s; break; }
        if (!def)
            continue;
        const QPointF c = m_hexArea->cellCenterByIndex(def->cell);
        h->setGeometry(static_cast<int>(c.x() - wW / 2.0),
                       static_cast<int>(c.y() - wH / 2.0), wW, wH);
    }
}

void DiseaseMenu::onHexClicked(const QString &id)
{
    m_selectedSkill = id;
    for (HexNode *h : m_hexes)
        h->setSelected(h->skillId() == id);
    updateInfoPanel();
}

static const SkillDef *findDef(const QVector<SkillDef> &defs, const QString &id)
{
    for (const SkillDef &s : defs)
        if (s.id == id)
            return &s;
    return nullptr;
}

void DiseaseMenu::updateInfoPanel()
{
    int page = skillPageForTab(m_tab);
    if (page < 0) {
        m_statBars->setPreview(DiseaseStats());
        return;
    }

    if (m_selectedSkill.isEmpty()) {
        m_statBars->setPreview(DiseaseStats());
        static const char *titles[3] = {"请选择传播途径特性", "请选择病症特性", "请选择能力特性"};
        static const char *bodies[3] = {
            "花费 DNA 点数来进化新的传播途径吧！\n\n这将增强病毒的传染性，使其更容易扩散到世界各地。",
            "花费 DNA 点数来进化疾病的杀伤力吧！\n\n强化病症可以提升严重性与致命性。",
            "花费 DNA 点数来自定义疾病的强度吧！\n\n增强抗性、干扰解药研发等等。"};
        m_infoTitle->setText(QString::fromUtf8(titles[page]));
        m_infoBody->setText(QString::fromUtf8(bodies[page]));
        m_evolveBtn->setText("进化");
        m_evolveBtn->setEnabled(false);
        return;
    }

    const SkillDef *s = findDef(m_core->skills(page), m_selectedSkill);
    if (!s)
        return;

    QStringList bonus;
    if (s->dInfectivity != 0.0)
        bonus << QString("传染性 +%1").arg(s->dInfectivity, 0, 'f', 2);
    if (s->dSeverity != 0.0)
        bonus << QString("严重性 +%1").arg(s->dSeverity, 0, 'f', 2);
    if (s->dLethality != 0.0)
        bonus << QString("致命性 +%1").arg(s->dLethality, 0, 'f', 2);
    if (s->dCureSlow != 0.0)
        bonus << QString("减缓解药研发 +%1").arg(s->dCureSlow, 0, 'f', 2);

    const bool unlocked = m_core->isUnlocked(s->id);
    const bool prereqMet = m_core->prereqsMet(s->id);

    // 选中未解锁技能：在条形上以浅色预览将增加的属性值
    DiseaseStats preview;
    if (!unlocked) {
        preview.infectivity = s->dInfectivity;
        preview.severity = s->dSeverity;
        preview.lethality = s->dLethality;
    }
    m_statBars->setPreview(preview);

    QString lock;
    if (!unlocked && !prereqMet) {
        const QStringList miss = m_core->missingPrereqNames(s->id);
        lock = QString("\n\n⚠ 需先解锁前置：%1").arg(miss.join("、"));
    }

    m_infoTitle->setText(s->name);
    m_infoBody->setText(s->desc + "\n\n"
                        + bonus.join("\n")
                        + QString("\n\n消耗 DNA：%1").arg(s->cost)
                        + lock
                        + (unlocked ? "\n\n（已进化）" : ""));

    if (unlocked) {
        m_evolveBtn->setText("退化");
        m_evolveBtn->setEnabled(true);
    } else if (!prereqMet) {
        m_evolveBtn->setText("未解锁前置");
        m_evolveBtn->setEnabled(false);
    } else {
        m_evolveBtn->setText("进化");
        m_evolveBtn->setEnabled(m_core->canAfford(s->id));
    }
}

void DiseaseMenu::onEvolveClicked()
{
    if (m_selectedSkill.isEmpty())
        return;
    if (m_core->isUnlocked(m_selectedSkill))
        m_core->devolveSkill(m_selectedSkill);
    else
        m_core->evolveSkill(m_selectedSkill);
    // 核心会发出 skillsChanged / dnaPointsChanged / diseaseStatsChanged 触发刷新
}

void DiseaseMenu::refreshStats()
{
    m_statBars->setStats(m_core->diseaseStats());
}

void DiseaseMenu::refreshDNA()
{
    m_statBars->setDNA(m_core->dnaPoints());
    // 刷新六边形的可购买状态
    for (HexNode *h : m_hexes)
        h->setState(m_core->isUnlocked(h->skillId()), m_core->isBuyable(h->skillId()));
    updateInfoPanel();
}

void DiseaseMenu::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    QLinearGradient g(0, 0, 0, height());
    g.setColorAt(0.0, QColor(110, 18, 30, 135)); // 顶部偏暗红
    g.setColorAt(1.0, QColor(205, 40, 95, 180));  // 底部偏品红
    p.fillRect(rect(), g);
}

void DiseaseMenu::resizeEvent(QResizeEvent *)
{
    layoutHexes();
}

void DiseaseMenu::hideEvent(QHideEvent *event)
{
    if (m_videoBg)
        m_videoBg->stop(); // 菜单关闭后不再后台解码视频
    QWidget::hideEvent(event);
}
