#include "mainwindow.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QStackedLayout>
#include <QFrame>
#include <QDate>
#include <QLocale>
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

    // ==========================================
    // 页面 0: 主游戏视图
    // ==========================================
    QWidget *gameView = new QWidget();
    QVBoxLayout *gameLayout = new QVBoxLayout(gameView);
    gameLayout->setContentsMargins(0, 0, 0, 0);
    gameLayout->setSpacing(0);

    // --- 顶部状态栏 ---
    QFrame *topBar = new QFrame();
    m_gameTopBar = topBar;
    topBar->setObjectName("TopBar");
    QHBoxLayout *topLayout = new QHBoxLayout(topBar);

    QPushButton *btnSettings = new QPushButton("⚙");
    btnSettings->setObjectName("IconBtn");
    QLabel *lblNews = new QLabel("News");
    lblNews->setObjectName("NewsLabel");

    // 右侧：日期 + 其下方三个速度按钮
    QWidget *rightBox = new QWidget();
    QVBoxLayout *rightLayout = new QVBoxLayout(rightBox);
    rightLayout->setContentsMargins(0, 0, 0, 0);
    rightLayout->setSpacing(4);

    QLabel *lblDate = new QLabel("01  06  2026");
    lblDate->setObjectName("DateLabel");
    lblDate->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

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

    rightLayout->addWidget(lblDate);
    rightLayout->addLayout(speedLayout);

    topLayout->addWidget(btnSettings);
    topLayout->addWidget(lblNews, 1);
    topLayout->addWidget(rightBox);

    // --- 底部状态栏 ---
    QFrame *bottomBar = new QFrame();
    m_gameBottomBar = bottomBar;
    bottomBar->setObjectName("BottomBar");
    QHBoxLayout *bottomLayout = new QHBoxLayout(bottomBar);

    QLabel *lblDNA = new QLabel("DNA: 0");
    lblDNA->setObjectName("DNALabel");

    QPushButton *btnDisease = new QPushButton("疾病概况");
    btnDisease->setObjectName("MenuBtnRed");
    QPushButton *btnWorld = new QPushButton("世界概况");
    btnWorld->setObjectName("MenuBtnBlue");

    QLabel *lblCure = new QLabel("解药研发 0%");
    lblCure->setObjectName("CureLabel");
    lblCure->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

    bottomLayout->addWidget(lblDNA);
    bottomLayout->addStretch();
    bottomLayout->addWidget(btnDisease);
    bottomLayout->addWidget(btnWorld);
    bottomLayout->addStretch();
    bottomLayout->addWidget(lblCure);

    m_popBar->setObjectName("PopBar");

    gameLayout->addWidget(topBar);
    gameLayout->addWidget(m_map, 1);
    gameLayout->addWidget(bottomBar);
    gameLayout->addWidget(m_popBar); // 底部人口占比线

    mainStack->addWidget(gameView); // Index 0

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
    // 初始化世界 + 连接 + 样式
    // ==========================================
    m_core->initWorld();
    m_regionSnapshot = m_core->regions();
    m_worldTotal = 0;
    for (const RegionData &r : m_regionSnapshot)
        m_worldTotal += r.totalPopulation;
    m_worldAlive = m_worldTotal;

    setupConnections();
    applyStyle();

    connect(btnDisease, &QPushButton::clicked, this, &MainWindow::openDiseaseMenu);
    connect(btnWorld, &QPushButton::clicked, this, &MainWindow::openWorldMenu);

    // 速度按钮
    connect(m_btnPause, &QPushButton::clicked, this, &MainWindow::setSpeedPaused);
    connect(m_btnNormal, &QPushButton::clicked, this, &MainWindow::setSpeedNormal);
    connect(m_btnFast, &QPushButton::clicked, this, &MainWindow::setSpeedFast);

    refreshBottomBar();
    updateDayUI(0); // 初始显示开局日期（真实日期）

    // 默认正常速度（时间在选定地区后才真正流动）
    setSpeedNormal();

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

    // 地图点击 -> 底部条切换
    connect(m_map, &MapWidget::regionSelected, this, &MainWindow::onRegionSelected);
    connect(m_map, &MapWidget::oceanClicked, this, &MainWindow::onOceanClicked);
    connect(m_map, &MapWidget::startRequested, this, &MainWindow::onStartRequested);

    // 新闻
    connect(m_core, &GameCore::newsGenerated, this, [this](QString text) {
        if (QLabel *newsLabel = this->findChild<QLabel *>("NewsLabel"))
            newsLabel->setText(text);
    });
}

// ==========================================
// 引擎 -> 界面
// ==========================================
void MainWindow::updateDayUI(int day)
{
    QDate currentDate = m_core->startDate().addDays(day);
    if (QLabel *dateLabel = this->findChild<QLabel *>("DateLabel"))
        dateLabel->setText(currentDate.toString("dd  MM  yyyy"));
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
    const QList<QLabel *> dnaLabels = this->findChildren<QLabel *>("DNALabel");
    for (QLabel *label : dnaLabels)
        label->setText(QString("DNA: %1").arg(shown));
}

void MainWindow::updateCure(double percent)
{
    if (QLabel *lblCure = this->findChild<QLabel *>("CureLabel"))
        lblCure->setText(QString("解药研发 %1%").arg(QString::number(percent, 'f', 1)));
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
    if (m_selectedRegion >= 0 && m_selectedRegion < m_regionSnapshot.size()) {
        const RegionData &r = m_regionSnapshot[m_selectedRegion];
        m_popBar->setData(r.name, r.healthy(), r.infectedCount, r.deadCount);
    } else {
        long long healthy = m_worldTotal - m_worldInfected - m_worldDead;
        m_popBar->setData("全球", healthy, m_worldInfected, m_worldDead);
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

    // 隐藏游戏自身的上下栏，避免半透明区域透出游戏 UI（仅保留地图作背景）
    if (m_gameTopBar) m_gameTopBar->hide();
    if (m_gameBottomBar) m_gameBottomBar->hide();
    if (m_popBar) m_popBar->hide();

    m_diseaseMenu->setGeometry(m_centralWidget->rect());
    m_diseaseMenu->openMenu();
}

void MainWindow::closeDiseaseMenu()
{
    m_diseaseMenu->hide();
    if (m_gameTopBar) m_gameTopBar->show();
    if (m_gameBottomBar) m_gameBottomBar->show();
    if (m_popBar) m_popBar->show();
    applySpeed(m_savedSpeed); // 恢复进入前的流速
}

void MainWindow::openWorldMenu()
{
    m_savedSpeed = m_currentSpeed;
    m_core->setSpeed(0); // 进入菜单暂停

    if (m_gameTopBar) m_gameTopBar->hide();
    if (m_gameBottomBar) m_gameBottomBar->hide();
    if (m_popBar) m_popBar->hide();

    m_worldMenu->setGeometry(m_centralWidget->rect());
    m_worldMenu->openMenu();
}

void MainWindow::closeWorldMenu()
{
    m_worldMenu->hide();
    if (m_gameTopBar) m_gameTopBar->show();
    if (m_gameBottomBar) m_gameBottomBar->show();
    if (m_popBar) m_popBar->show();
    applySpeed(m_savedSpeed);
}

void MainWindow::resizeEvent(QResizeEvent *event)
{
    QMainWindow::resizeEvent(event);
    if (m_diseaseMenu && m_diseaseMenu->isVisible())
        m_diseaseMenu->setGeometry(m_centralWidget->rect());
    if (m_worldMenu && m_worldMenu->isVisible())
        m_worldMenu->setGeometry(m_centralWidget->rect());
}

bool MainWindow::eventFilter(QObject *watched, QEvent *event)
{
    if (event->type() == QEvent::KeyPress) {
        QKeyEvent *ke = static_cast<QKeyEvent *>(event);
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

        #PopBar { background-color: rgba(15, 22, 30, 235); border-top: 1px solid #333; }

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
    )");
}
