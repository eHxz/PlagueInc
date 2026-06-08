#include "mainwindow.h"
#include "soundmanager.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QStackedLayout>
#include <QFrame>
#include <QDate>
#include <QLocale>
#include <QLineEdit>
#include <QMenu>
#include <QMessageBox>
#include <QResizeEvent>
#include <QKeyEvent>
#include <QApplication>
#include <QTimer>
#include <QDebug>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_core(new GameCore(this))
    , m_map(new MapWidget(this))
    , m_popBar(new PopulationBar(this))
{
    this->resize(1100, 760);
    this->setWindowTitle("Plague Inc. Clone");

    QWidget *centralWidget = new QWidget(this);
    m_centralWidget = centralWidget;
    this->setCentralWidget(centralWidget);
    QStackedLayout *mainStack = new QStackedLayout(centralWidget);
    m_mainStack = mainStack;

    // ==========================================
    // 页面 0: 主游戏视图
    // ==========================================
    QWidget *gameView = new QWidget();
    m_gameView = gameView;
    QVBoxLayout *gameLayout = new QVBoxLayout(gameView);
    gameLayout->setContentsMargins(0, 0, 0, 0);
    gameLayout->setSpacing(0);
    gameLayout->addWidget(m_map, 1); // 地图铺满整个游戏视图，HUD 浮于其上

    // HUD 以“地图视图本身（QGraphicsView）”为父，而不是它的 viewport：
    // QAbstractScrollArea 中、非 viewport 的子控件会叠加在 viewport 之上，且不参与
    // 场景的局部重绘循环——因此它们不会被地图刷新擦掉，始终稳定显示（这是叠加 HUD 的标准做法）。
    // 去掉视图边框，使 viewport 与视图等大、坐标对齐（HUD 用归一化/像素坐标摆放时无偏移）。
    m_map->setFrameShape(QFrame::NoFrame);
    QWidget *hud = m_map;

    // ---------- HUD 覆盖层（绝对定位，半透明绘制，点击进入对应菜单）----------
    // 左上角：新闻
    m_newsPanel = new HudPanel(hud);
    m_newsPanel->setPixmap(QPixmap(":/mainui/news.png"));
    m_newsPanel->setBgOpacity(0.82);
    m_newsSlot = m_newsPanel->addSlot(QRectF(0.12, 0.10, 0.74, 0.55),
                                      Qt::AlignLeft, QColor(225, 236, 245), 0.40, true);

    // 左下角：DNA（点击 -> 疾病概况）
    m_diseasePanel = new HudPanel(hud);
    m_diseasePanel->setPixmap(QPixmap(":/mainui/disease.png"));
    m_diseasePanel->setBgOpacity(0.85);
    m_dnaSlot = m_diseasePanel->addSlot(QRectF(0.20, 0.56, 0.72, 0.34),
                                        Qt::AlignHCenter, QColor(255, 235, 238), 0.26, true);
    connect(m_diseasePanel, &HudPanel::clicked, this, &MainWindow::openDiseaseMenu);

    // 右下角：解药研发进度（点击 -> 世界概况）
    m_curePanel = new HudPanel(hud);
    m_curePanel->setPixmap(QPixmap(":/mainui/cure.png"));
    m_curePanel->setBgOpacity(0.85);
    m_cureSlot = m_curePanel->addSlot(QRectF(0.20, 0.56, 0.72, 0.34),
                                      Qt::AlignHCenter, QColor(225, 245, 255), 0.26, true);
    connect(m_curePanel, &HudPanel::clicked, this, &MainWindow::openWorldMenu);

    // 下方正中：世界条（地区名 + 感染 + 死亡，点击 -> 世界概况）
    m_worldPanel = new HudPanel(hud);
    m_worldPanel->setPixmap(QPixmap(":/mainui/world.png"));
    m_worldPanel->setBgOpacity(0.86);
    m_wName     = m_worldPanel->addSlot(QRectF(0.215, 0.06, 0.62, 0.26),
                                        Qt::AlignLeft, QColor(235, 244, 252), 0.17, true);
    m_wInfected = m_worldPanel->addSlot(QRectF(0.235, 0.60, 0.215, 0.24),
                                        Qt::AlignHCenter, QColor(255, 205, 120), 0.16, true);
    m_wDead     = m_worldPanel->addSlot(QRectF(0.49, 0.60, 0.30, 0.24),
                                        Qt::AlignHCenter, QColor(235, 235, 240), 0.16, true);
    connect(m_worldPanel, &HudPanel::clicked, this, &MainWindow::openWorldMenu);

    // 感染进度条（缩短后置于世界条正下方）
    m_popBar->setObjectName("PopBar");
    m_popBar->setParent(hud);

    // 右上角：日期 + 设置 + 速度按钮
    m_topRight = new QWidget(hud);
    m_topRight->setObjectName("TopRightBox");
    QVBoxLayout *trLayout = new QVBoxLayout(m_topRight);
    trLayout->setContentsMargins(8, 6, 8, 6);
    trLayout->setSpacing(4);

    QHBoxLayout *dateRow = new QHBoxLayout();
    dateRow->setSpacing(6);
    m_dateLabel = new QLabel("01  06  2026");
    m_dateLabel->setObjectName("DateLabel");
    m_dateLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    m_settingsBtn = new QPushButton("⚙");
    m_settingsBtn->setObjectName("IconBtn");
    m_settingsBtn->setFixedSize(30, 28);
    m_settingsBtn->setCursor(Qt::PointingHandCursor);
    dateRow->addStretch();
    dateRow->addWidget(m_dateLabel);
    dateRow->addWidget(m_settingsBtn);

    QHBoxLayout *speedLayout = new QHBoxLayout();
    speedLayout->setSpacing(4);
    m_btnPause = new QPushButton("⏸");
    m_btnNormal = new QPushButton("▶");
    m_btnFast = new QPushButton("▶▶");
    for (QPushButton *b : {m_btnPause, m_btnNormal, m_btnFast}) {
        b->setObjectName("SpeedBtn");
        b->setCheckable(true);
        b->setFixedSize(40, 26);
    }
    m_btnPause->setToolTip("暂停");
    m_btnNormal->setToolTip("正常");
    m_btnFast->setToolTip("两倍速");
    speedLayout->addStretch();
    speedLayout->addWidget(m_btnPause);
    speedLayout->addWidget(m_btnNormal);
    speedLayout->addWidget(m_btnFast);

    trLayout->addLayout(dateRow);
    trLayout->addLayout(speedLayout);

    connect(m_settingsBtn, &QPushButton::clicked, this, &MainWindow::openSettings);

    mainStack->addWidget(gameView); // Index 0

    // ==========================================
    // 命名页：初始进入游戏时为病原体命名
    // ==========================================
    QWidget *namePage = new QWidget();
    m_namePage = namePage;
    namePage->setObjectName("NamePage");
    QVBoxLayout *nameLayout = new QVBoxLayout(namePage);
    nameLayout->addStretch();

    QLabel *nameTitle = new QLabel("为你的病原体命名");
    nameTitle->setObjectName("NameTitle");
    nameTitle->setAlignment(Qt::AlignHCenter);
    QLabel *nameHint = new QLabel("输入名称后，点击地图上的地区即可开始这场全球瘟疫");
    nameHint->setObjectName("NameHint");
    nameHint->setAlignment(Qt::AlignHCenter);

    m_nameEdit = new QLineEdit();
    m_nameEdit->setObjectName("NameEdit");
    m_nameEdit->setPlaceholderText("例如：瘟疫");
    m_nameEdit->setMaxLength(20);
    m_nameEdit->setAlignment(Qt::AlignHCenter);
    m_nameEdit->setFixedWidth(380);

    QPushButton *nameOk = new QPushButton("开  始");
    nameOk->setObjectName("NameOkBtn");
    nameOk->setFixedWidth(380);
    nameOk->setCursor(Qt::PointingHandCursor);

    nameLayout->addWidget(nameTitle);
    nameLayout->addSpacing(10);
    nameLayout->addWidget(nameHint);
    nameLayout->addSpacing(34);
    QHBoxLayout *editRow = new QHBoxLayout();
    editRow->addStretch(); editRow->addWidget(m_nameEdit); editRow->addStretch();
    nameLayout->addLayout(editRow);
    nameLayout->addSpacing(16);
    QHBoxLayout *btnRow = new QHBoxLayout();
    btnRow->addStretch(); btnRow->addWidget(nameOk); btnRow->addStretch();
    nameLayout->addLayout(btnRow);
    nameLayout->addStretch();

    mainStack->addWidget(namePage); // Index 1

    connect(nameOk, &QPushButton::clicked, this, &MainWindow::confirmDiseaseName);
    connect(m_nameEdit, &QLineEdit::returnPressed, this, &MainWindow::confirmDiseaseName);

    // ==========================================
    // 疾病菜单 / 世界概况菜单：半透明覆盖层（盖在游戏视图上，地图透出）
    // ==========================================
    m_diseaseMenu = new DiseaseMenu(m_core, centralWidget);
    m_diseaseMenu->hide();
    connect(m_diseaseMenu, &DiseaseMenu::closed, this, &MainWindow::closeDiseaseMenu);

    m_worldMenu = new WorldMenu(m_core, centralWidget);
    m_worldMenu->hide();
    connect(m_worldMenu, &WorldMenu::closed, this, &MainWindow::closeWorldMenu);

    // ==========================================
    // 报告弹窗（主页面正中央）：触发时暂停时间，点 OK 关闭后恢复流速
    // ==========================================
    m_noticePopup = new NoticePopup(centralWidget);
    m_noticePopup->hide();
    m_noticeManager = new NoticeManager(m_core, m_noticePopup, this);
    m_noticeManager->loadFromFile(":/notice.txt");
    connect(m_noticePopup, &NoticePopup::firstShown, this, [this]() {
        m_speedBeforeNotice = m_currentSpeed;
        m_core->setSpeed(0); // 暂停（不改流速高亮，便于关闭后恢复）
        m_noticePopup->setGeometry(m_centralWidget->rect());
        m_noticePopup->raise();
    });
    connect(m_noticePopup, &NoticePopup::allDismissed, this, [this]() {
        // 若仍有菜单打开（如概况页内进化触发的报告），保持暂停，待菜单关闭再恢复；
        // 否则恢复弹出前的流速。
        const bool menuOpen = (m_diseaseMenu && m_diseaseMenu->isVisible())
                              || (m_worldMenu && m_worldMenu->isVisible());
        if (menuOpen)
            m_core->setSpeed(0);
        else
            applySpeed(m_speedBeforeNotice);
    });

    // ==========================================
    // 初始化世界 + 连接 + 样式
    // ==========================================
    m_core->initWorld();
    m_regionSnapshot = m_core->regions();
    m_worldTotal = 0;
    for (const RegionData &r : m_regionSnapshot)
        m_worldTotal += r.totalPopulation;
    m_worldAlive = m_worldTotal;

    setupConnections();
    m_map->onDiseaseStatsChanged(m_core->diseaseStats()); // 初始传染性（initWorld 早于连接，补推一次）
    updateDNAPoints(m_core->dnaPoints()); // 初始 DNA（同上：连接前的发射会丢，补推一次）
    updateCure(m_core->cureProgress());   // 初始解药进度
    applyStyle();

    // 速度按钮
    connect(m_btnPause, &QPushButton::clicked, this, &MainWindow::setSpeedPaused);
    connect(m_btnNormal, &QPushButton::clicked, this, &MainWindow::setSpeedNormal);
    connect(m_btnFast, &QPushButton::clicked, this, &MainWindow::setSpeedFast);

    refreshBottomBar();
    updateDayUI(0); // 初始显示开局日期（真实日期）

    // 默认正常速度（时间在选定地区后才真正流动）
    setSpeedNormal();

    // 初始停在命名页，命名后再进入主游戏
    mainStack->setCurrentWidget(namePage); // 初始停在命名页，命名后再进入主游戏

    // 监听键盘以捕获作弊码
    qApp->installEventFilter(this);
}

MainWindow::~MainWindow() {}

// ==========================================
// 信号连接
// ==========================================
void MainWindow::setupConnections()
{
    connect(m_core, &GameCore::dayPassed, this, &MainWindow::updateDayUI);
    connect(m_core, &GameCore::globalStatsUpdated, this, &MainWindow::updateStatsUI);
    connect(m_core, &GameCore::dnaPointsChanged, this, &MainWindow::updateDNAPoints);
    connect(m_core, &GameCore::cureProgressChanged, this, &MainWindow::updateCure);
    connect(m_core, &GameCore::gameOver, this, &MainWindow::onGameOver);

    // 地区数据 -> 地图 + 本窗口快照
    connect(m_core, &GameCore::regionUpdated, m_map, &MapWidget::onRegionUpdated);
    connect(m_core, &GameCore::regionUpdated, this, &MainWindow::onRegionUpdated);

    // DNA 气泡
    connect(m_core, &GameCore::newRegionInfected, m_map, &MapWidget::onNewRegionInfected);
    connect(m_core, &GameCore::infectionSurge, m_map, &MapWidget::onInfectionSurge);
    connect(m_map, &MapWidget::dnaCollected, m_core, &GameCore::collectDNA);
    // 气泡寿命随游戏时间流逝（受倍速/暂停影响）；同时驱动海空航班发车
    connect(m_core, &GameCore::dayPassed, m_map, &MapWidget::onDayTick);

    // 海空交通：传染性 -> 载毒概率；受感染航班抵达 -> 感染目标地区
    connect(m_core, &GameCore::diseaseStatsChanged, m_map, &MapWidget::onDiseaseStatsChanged);
    connect(m_map, &MapWidget::routeInfection, m_core, &GameCore::seedExternalInfection);
    // 政府封锁（formula 形式三）：关闭机场/港口 -> 影响海空发车与图标
    connect(m_core, &GameCore::lockdownChanged, m_map, &MapWidget::onLockdownChanged);
    // 游戏速度/暂停（含进入菜单）-> 海空载具动画同步加速/冻结
    connect(m_core, &GameCore::speedChanged, m_map, &MapWidget::onGameSpeedChanged);

    // 地图点击 -> 底部条切换
    connect(m_map, &MapWidget::regionSelected, this, &MainWindow::onRegionSelected);
    connect(m_map, &MapWidget::oceanClicked, this, &MainWindow::onOceanClicked);
    connect(m_map, &MapWidget::startRequested, this, &MainWindow::onStartRequested);

    // 新闻
    connect(m_core, &GameCore::newsGenerated, this, [this](QString text) {
        if (m_newsPanel)
            m_newsPanel->setText(m_newsSlot, text);
    });
}

// ==========================================
// 引擎 -> 界面
// ==========================================
void MainWindow::updateDayUI(int day)
{
    QDate currentDate = m_core->startDate().addDays(day);
    if (m_dateLabel)
        m_dateLabel->setText(currentDate.toString("dd  MM  yyyy"));
}

void MainWindow::updateStatsUI(long long infected, long long dead, long long alive, long long total)
{
    m_worldInfected = infected;
    m_worldDead = dead;
    m_worldAlive = alive;
    m_worldTotal = total;

    if (m_selectedRegion < 0)
        refreshBottomBar();
}

void MainWindow::updateDNAPoints(int points)
{
    const QString shown = (points >= 999999) ? QStringLiteral("∞") : QString::number(points);
    if (m_diseasePanel)
        m_diseasePanel->setText(m_dnaSlot, shown);
}

void MainWindow::updateCure(double percent)
{
    if (m_curePanel)
        m_curePanel->setText(m_cureSlot, QString("%1%").arg(QString::number(percent, 'f', 1)));
}

void MainWindow::onRegionUpdated(int regionId, RegionData data)
{
    if (regionId >= 0 && regionId < m_regionSnapshot.size())
        m_regionSnapshot[regionId] = data;
    if (regionId == m_selectedRegion)
        refreshBottomBar();
}

void MainWindow::onRegionSelected(int regionId)
{
    // 选中地区只切换底部条；投放零号病人改由“开始气泡”触发
    m_selectedRegion = regionId;
    refreshBottomBar();
}

void MainWindow::onStartRequested(int regionId)
{
    if (m_core->isSeeded())
        return;
    m_core->seedInfection(regionId); // 时间从此刻（真实日期）开始
    m_map->onGameStarted();
    m_selectedRegion = regionId;
    refreshBottomBar();
}

void MainWindow::onOceanClicked()
{
    m_selectedRegion = -1;
    refreshBottomBar();
}

void MainWindow::refreshBottomBar()
{
    QString name;
    long long healthy = 0, infected = 0, dead = 0;
    if (m_selectedRegion >= 0 && m_selectedRegion < m_regionSnapshot.size()) {
        const RegionData &r = m_regionSnapshot[m_selectedRegion];
        name = r.name; healthy = r.healthy(); infected = r.infectedCount; dead = r.deadCount;
    } else {
        name = "全球";
        infected = m_worldInfected; dead = m_worldDead;
        healthy = m_worldTotal - m_worldInfected - m_worldDead;
    }
    m_popBar->setData(name, healthy, infected, dead);

    // world.png 世界条：地区名 + 感染人数 + 死亡人数
    QLocale loc(QLocale::English);
    if (m_worldPanel) {
        m_worldPanel->setText(m_wName, name);
        m_worldPanel->setText(m_wInfected, loc.toString(infected));
        m_worldPanel->setText(m_wDead, loc.toString(dead));
    }
}

void MainWindow::onGameOver(bool humansSurvived, QString message)
{
    highlightSpeed(0);
    QMessageBox box(this);
    box.setWindowTitle(humansSurvived ? "人类胜利" : "病毒胜利");
    box.setText(message);
    box.setIcon(humansSurvived ? QMessageBox::Information : QMessageBox::Critical);
    box.exec();
}

// ==========================================
// 速度控制
// ==========================================
void MainWindow::setSpeedPaused() { applySpeed(0); }
void MainWindow::setSpeedNormal() { applySpeed(1); }
void MainWindow::setSpeedFast()   { applySpeed(2); }

void MainWindow::applySpeed(int which)
{
    m_currentSpeed = which;
    switch (which) {
    case 0: m_core->setSpeed(0); break;
    case 1: m_core->setSpeed(1000); break;
    case 2: m_core->setSpeed(500); break;
    }
    highlightSpeed(which);
}

void MainWindow::highlightSpeed(int which)
{
    if (m_btnPause) m_btnPause->setChecked(which == 0);
    if (m_btnNormal) m_btnNormal->setChecked(which == 1);
    if (m_btnFast) m_btnFast->setChecked(which == 2);
}

// ==========================================
// 疾病菜单：进入自动暂停，退出恢复原速
// ==========================================
void MainWindow::openDiseaseMenu()
{
    m_savedSpeed = m_currentSpeed;
    m_core->setSpeed(0); // 暂停，但不改变流速高亮，便于退出后恢复

    setHudVisible(false); // 隐藏 HUD，仅保留地图作背景
    m_diseaseMenu->setGeometry(m_centralWidget->rect());
    m_diseaseMenu->openMenu();
}

void MainWindow::closeDiseaseMenu()
{
    m_diseaseMenu->hide();
    setHudVisible(true);
    applySpeed(m_savedSpeed); // 恢复进入前的流速
}

void MainWindow::openWorldMenu()
{
    m_savedSpeed = m_currentSpeed;
    m_core->setSpeed(0); // 进入菜单暂停

    setHudVisible(false);
    m_worldMenu->setGeometry(m_centralWidget->rect());
    m_worldMenu->openMenu();
}

void MainWindow::closeWorldMenu()
{
    m_worldMenu->hide();
    setHudVisible(true);
    applySpeed(m_savedSpeed);
}

// HUD 整体显示/隐藏（进入/退出菜单时调用）
void MainWindow::setHudVisible(bool on)
{
    for (QWidget *w : {static_cast<QWidget *>(m_newsPanel),
                       static_cast<QWidget *>(m_diseasePanel),
                       static_cast<QWidget *>(m_curePanel),
                       static_cast<QWidget *>(m_worldPanel),
                       static_cast<QWidget *>(m_popBar),
                       m_topRight}) {
        if (w) w->setVisible(on);
    }
    if (on)
        layoutHud();
}

// 摆放 HUD 覆盖层：新闻(左上)、日期/设置/速度(右上)、DNA(左下)、世界条+进度条(下中)、解药(右下)。
void MainWindow::layoutHud()
{
    if (!m_map)
        return;
    QWidget *host = m_map; // HUD 以视图本身为父，按其尺寸摆放（NoFrame 下与 viewport 等大）
    const int W = host->width();
    const int H = host->height();
    const int margin = 12;

    auto aspectW = [](HudPanel *p, int h) -> int {
        const QSize s = p->pixmapSize();
        return (s.height() > 0) ? qRound(double(h) * s.width() / s.height()) : h * 3;
    };

    // 左上角：新闻
    if (m_newsPanel) {
        const int h = qBound(34, int(H * 0.058), 54);
        int w = aspectW(m_newsPanel, h);
        w = qMin(w, int(W * 0.52));
        m_newsPanel->setGeometry(margin, margin, w, h);
        m_newsPanel->raise();
    }
    // 右上角：日期 + 设置 + 速度
    if (m_topRight) {
        m_topRight->adjustSize();
        const int w = m_topRight->sizeHint().width();
        const int h = m_topRight->sizeHint().height();
        m_topRight->setGeometry(W - margin - w, margin, w, h);
        m_topRight->raise();
    }

    // 下方正中：世界条 + 其下的进度条
    int worldBottom = H - margin;
    int popH = 22;
    int worldH = qBound(58, int(H * 0.12), 116);
    int worldW = m_worldPanel ? aspectW(m_worldPanel, worldH) : 0;
    worldW = qMin(worldW, int(W * 0.56));
    // 若按宽度受限，则反推高度，保持比例
    if (m_worldPanel) {
        const QSize ps = m_worldPanel->pixmapSize();
        if (ps.width() > 0)
            worldH = qRound(double(worldW) * ps.height() / ps.width());
    }
    const int popW = qMax(180, int(worldW * 0.86));
    const int worldX = (W - worldW) / 2;
    const int popY = worldBottom - popH;
    const int worldY = popY - 2 - worldH;
    if (m_worldPanel) {
        m_worldPanel->setGeometry(worldX, worldY, worldW, worldH);
        m_worldPanel->raise();
    }
    if (m_popBar) {
        m_popBar->setGeometry((W - popW) / 2, popY, popW, popH);
        m_popBar->raise();
    }

    // 左下角：DNA
    if (m_diseasePanel) {
        const int h = qBound(64, int(H * 0.13), 124);
        int w = aspectW(m_diseasePanel, h);
        w = qMin(w, int(W * 0.30));
        const QSize ps = m_diseasePanel->pixmapSize();
        const int hh = (ps.width() > 0) ? qRound(double(w) * ps.height() / ps.width()) : h;
        m_diseasePanel->setGeometry(margin, H - margin - hh, w, hh);
        m_diseasePanel->raise();
    }
    // 右下角：解药
    if (m_curePanel) {
        const int h = qBound(64, int(H * 0.13), 124);
        int w = aspectW(m_curePanel, h);
        w = qMin(w, int(W * 0.30));
        const QSize ps = m_curePanel->pixmapSize();
        const int hh = (ps.width() > 0) ? qRound(double(w) * ps.height() / ps.width()) : h;
        m_curePanel->setGeometry(W - margin - w, H - margin - hh, w, hh);
        m_curePanel->raise();
    }
}

// ==========================================
// 命名页 / 设置（返回·重开）
// ==========================================
void MainWindow::confirmDiseaseName()
{
    if (m_nameEdit)
        m_core->setDiseaseName(m_nameEdit->text()); // 空则回退为默认名
    if (m_mainStack && m_gameView)
        m_mainStack->setCurrentWidget(m_gameView);
    layoutHud();
    if (m_noticeManager)
        m_noticeManager->onGameStarted(); // 报告 1（欢迎）+ 2（选择起始地区）
}

void MainWindow::openSettings()
{
    QMenu menu(this);
    QAction *back = menu.addAction("返回");   // 关闭设置，继续游戏
    const bool muted = SoundManager::instance().isMuted();
    QAction *sound = menu.addAction(muted ? "开启音效" : "静音");
    QAction *restart = menu.addAction("重开"); // 重置并回到命名页
    QAction *chosen = menu.exec(QCursor::pos());
    if (chosen == restart)
        restartGame();
    else if (chosen == sound)
        SoundManager::instance().setMuted(!muted);
    else
        Q_UNUSED(back); // 返回：直接关闭弹出菜单
}

void MainWindow::restartGame()
{
    // 关闭可能打开的覆盖层
    if (m_diseaseMenu && m_diseaseMenu->isVisible())
        closeDiseaseMenu();
    if (m_worldMenu && m_worldMenu->isVisible())
        closeWorldMenu();

    if (m_noticePopup)
        m_noticePopup->dismissAll(); // 清掉可能残留的报告弹窗
    if (m_noticeManager)
        m_noticeManager->reset();    // 清空已触发记录

    m_core->initWorld();   // 重置世界（含 m_seeded=false、日期、解药、历史）
    m_map->resetMap();     // 清空气泡与点阵密度

    m_regionSnapshot = m_core->regions();
    m_worldTotal = 0;
    for (const RegionData &r : m_regionSnapshot)
        m_worldTotal += r.totalPopulation;
    m_worldInfected = 0;
    m_worldDead = 0;
    m_worldAlive = m_worldTotal;
    m_selectedRegion = -1;

    if (m_nameEdit) {
        m_nameEdit->clear();
        m_nameEdit->setFocus();
    }
    refreshBottomBar();
    updateDayUI(0);
    applySpeed(1);

    if (m_mainStack && m_namePage)
        m_mainStack->setCurrentWidget(m_namePage); // 回到命名页
}

void MainWindow::showEvent(QShowEvent *event)
{
    QMainWindow::showEvent(event);
    // 显示到最终尺寸后再摆放一次（构造期的早期 resize 尺寸可能还不是最终值）
    QTimer::singleShot(0, this, [this]() { layoutHud(); });
}

void MainWindow::resizeEvent(QResizeEvent *event)
{
    QMainWindow::resizeEvent(event);
    layoutHud();
    if (m_diseaseMenu && m_diseaseMenu->isVisible())
        m_diseaseMenu->setGeometry(m_centralWidget->rect());
    if (m_worldMenu && m_worldMenu->isVisible())
        m_worldMenu->setGeometry(m_centralWidget->rect());
    if (m_noticePopup && m_noticePopup->isVisible())
        m_noticePopup->setGeometry(m_centralWidget->rect());
}

bool MainWindow::eventFilter(QObject *watched, QEvent *event)
{
    // 地图视图（或其 viewport）改变尺寸时重新摆放 HUD（HUD 以视图为父）
    if (event->type() == QEvent::Resize && m_map &&
        (watched == m_map || watched == m_map->viewport()))
        layoutHud();

    if (event->type() == QEvent::KeyPress) {
        QKeyEvent *ke = static_cast<QKeyEvent *>(event);
        if (ke->key() == Qt::Key_F9) {        // F9：一键开/关无限 DNA（输入法拦不住）
            m_core->toggleInfiniteDNA();
            return true;
        }
        QString t = ke->text().toLower();
        if (t.size() == 1 && t[0].isLetter()) {
            m_cheatBuf += t;
            const QString code = "cheat";
            if (m_cheatBuf.size() > code.size())
                m_cheatBuf = m_cheatBuf.right(code.size());
            if (m_cheatBuf == code) {
                m_core->setInfiniteDNA(true);
                m_cheatBuf.clear();
            }
        }
    }
    return QMainWindow::eventFilter(watched, event);
}

// ==========================================
// 样式
// ==========================================
void MainWindow::applyStyle()
{
    this->setStyleSheet(R"(
        QMainWindow { background-color: #000000; }

        #TopBar, #BottomBar {
            background-color: rgba(20, 30, 40, 220);
            border-top: 1px solid #444;
            border-bottom: 1px solid #444;
        }

        QLabel {
            color: #E0E0E0;
            font-family: "Microsoft YaHei", sans-serif;
            font-weight: bold;
            font-size: 16px;
        }

        #DateLabel { font-size: 20px; color: #FFFFFF; padding-right: 6px; }

        #NewsLabel {
            background-color: #000000;
            border: 1px solid #333;
            border-radius: 5px;
            padding: 5px;
            color: #00FFCC;
        }

        #SpeedBtn {
            background-color: #223344;
            color: #DDDDDD;
            border: 1px solid #556;
            border-radius: 4px;
            font-size: 13px;
        }
        #SpeedBtn:hover { background-color: #2e4a66; }
        #SpeedBtn:checked { background-color: #00AAFF; color: white; border: 1px solid #00CCFF; }

        #TopRightBox {
            background-color: rgba(18, 28, 38, 170);
            border: 1px solid rgba(90, 115, 140, 150);
            border-radius: 6px;
        }
        #IconBtn {
            background-color: rgba(40, 55, 70, 210); color: #ffffff;
            border: 1px solid #556; border-radius: 4px; font-size: 16px; font-weight: bold;
        }
        #IconBtn:hover { background-color: rgba(60, 82, 104, 230); }

        #MenuBtnRed, #MenuBtnBlue {
            font-size: 18px; font-weight: bold; padding: 10px 30px;
            border: none; color: white; border-radius: 2px;
        }
        #MenuBtnRed { background-color: #AA0000; }
        #MenuBtnRed:hover { background-color: #DD0000; }
        #MenuBtnBlue { background-color: #0088CC; }
        #MenuBtnBlue:hover { background-color: #00AAFF; }

        #DNALabel { color: #CFCFCF; font-size: 18px; padding-left: 15px; }
        #CureLabel { color: #9fd0ff; font-size: 18px; padding-right: 15px; }

        #PopBar { background-color: transparent; }

        #DiseaseMenu { background-color: rgba(60, 10, 15, 235); }
        #WorldMenu { background-color: rgba(10, 30, 50, 235); }

        #MenuTopBar { background-color: #DDDDDD; }
        #MenuTopBar QPushButton {
            background-color: transparent; color: #333333; font-weight: bold;
            font-size: 16px; padding: 15px 30px; border: none;
        }
        #MenuTopBar QPushButton:hover { background-color: #FFFFFF; }
        #ActiveTabRed { background-color: #AA0000; color: white; }
        #ActiveTabBlue { background-color: #00AADD; color: white; }
        #CloseBtn { color: #AA0000; font-size: 24px; font-weight: 900; }

        #InfoPanel {
            background-color: #050505; color: #DDAAAA; padding: 20px;
            border-right: 2px solid #333;
        }
        #WorldDataLabel { font-size: 24px; color: #FFFFFF; padding: 40px; }

        #NamePage { background-color: #0a0e14; }
        #NameTitle { font-size: 36px; color: #FFFFFF; font-weight: bold; }
        #NameHint { font-size: 16px; color: #7fa8cc; }
        #NameEdit {
            font-size: 22px; padding: 12px; border-radius: 6px;
            background-color: #16202c; color: #FFFFFF; border: 1px solid #2a4a66;
        }
        #NameEdit:focus { border: 1px solid #00AAFF; }
        #NameOkBtn {
            font-size: 22px; font-weight: bold; padding: 13px;
            color: #FFFFFF; background-color: #0088CC; border: none; border-radius: 6px;
        }
        #NameOkBtn:hover { background-color: #00AAFF; }
    )");
}
