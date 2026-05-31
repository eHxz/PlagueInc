#include "mainwindow.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QStackedLayout>
#include <QFrame>
#include <QProgressBar>
#include <QDate>
#include <QDebug>
#include <QGraphicsDropShadowEffect>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), m_core(new GameCore(this)), m_map(new MapWidget(this))
{
    // 1. 初始化基础设置
    this->resize(1024, 768);
    this->setWindowTitle("Plague Inc. Clone");

    // 2. 构建主界面容器 (使用 QStackedLayout 来实现页面覆盖切换)
    QWidget *centralWidget = new QWidget(this);
    this->setCentralWidget(centralWidget);
    QStackedLayout *mainStack = new QStackedLayout(centralWidget);

    // ==========================================
    // 页面 0: 主游戏视图 (地图与上下状态栏)
    // ==========================================
    QWidget *gameView = new QWidget();
    QVBoxLayout *gameLayout = new QVBoxLayout(gameView);
    gameLayout->setContentsMargins(0, 0, 0, 0);

    // --- 顶部状态栏 ---
    QFrame *topBar = new QFrame();
    topBar->setObjectName("TopBar");
    QHBoxLayout *topLayout = new QHBoxLayout(topBar);

    QPushButton *btnSettings = new QPushButton("⚙");
    btnSettings->setObjectName("IconBtn");
    QLabel *lblNews = new QLabel("News");
    lblNews->setObjectName("NewsLabel");
    QLabel *lblDate = new QLabel("31  05  2026");
    lblDate->setObjectName("DateLabel");
    lblDate->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

    topLayout->addWidget(btnSettings);
    topLayout->addWidget(lblNews, 1);
    topLayout->addWidget(lblDate);

    // --- 中间地图区域 ---
    // (B同学负责完善 MapWidget，这里直接加入)

    // --- 底部状态栏 ---
    QFrame *bottomBar = new QFrame();
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

    gameLayout->addWidget(topBar);
    gameLayout->addWidget(m_map, 1); // 地图占据剩余所有空间
    gameLayout->addWidget(bottomBar);

    mainStack->addWidget(gameView); // Index 0

    // ==========================================
    // 页面 1: 疾病概况菜单 (红色主题，参考图2、图3)
    // ==========================================
    QFrame *diseaseMenu = new QFrame();
    diseaseMenu->setObjectName("DiseaseMenu");
    QVBoxLayout *diseaseLayout = new QVBoxLayout(diseaseMenu);
    diseaseLayout->setContentsMargins(0, 0, 0, 0);

    // 疾病菜单顶部 Tab
    QFrame *diseaseTopBar = new QFrame();
    diseaseTopBar->setObjectName("MenuTopBar");
    QHBoxLayout *dtLayout = new QHBoxLayout(diseaseTopBar);
    QPushButton *btnD1 = new QPushButton("疾病概况");
    btnD1->setObjectName("ActiveTabRed");
    QPushButton *btnD2 = new QPushButton("传播途径");
    QPushButton *btnD3 = new QPushButton("发病症状");
    QPushButton *btnD4 = new QPushButton("特殊能力");
    QPushButton *btnCloseDisease = new QPushButton("✖");
    btnCloseDisease->setObjectName("CloseBtn");

    dtLayout->addWidget(btnD1);
    dtLayout->addWidget(btnD2);
    dtLayout->addWidget(btnD3);
    dtLayout->addWidget(btnD4);
    dtLayout->addStretch();
    dtLayout->addWidget(btnCloseDisease);

    // 疾病菜单中间内容 (占位)
    QWidget *diseaseContent = new QWidget();
    QHBoxLayout *dcLayout = new QHBoxLayout(diseaseContent);
    QLabel *leftPanel = new QLabel("开始地点: 未知\n\n开始日期:\nMay 31, 2026\n\n每日感染者人数:\n0\n\n每日死亡人数:\n0");
    leftPanel->setObjectName("InfoPanel");
    leftPanel->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    dcLayout->addWidget(leftPanel, 1);
    dcLayout->addStretch(3); // 右侧留空给六边形图标

    // 疾病菜单底部进度条
    QFrame *diseaseBottomBar = new QFrame();
    diseaseBottomBar->setObjectName("DiseaseBottom");
    QHBoxLayout *dbLayout = new QHBoxLayout(diseaseBottomBar);
    QLabel *lblDNA_Menu = new QLabel("DNA  0");
    lblDNA_Menu->setObjectName("DNALabel");

    QVBoxLayout *v1 = new QVBoxLayout(); v1->addWidget(new QLabel("传染性")); QProgressBar *p1 = new QProgressBar(); p1->setValue(10); p1->setTextVisible(false); v1->addWidget(p1);
    QVBoxLayout *v2 = new QVBoxLayout(); v2->addWidget(new QLabel("严重性")); QProgressBar *p2 = new QProgressBar(); p2->setValue(0); p2->setTextVisible(false); v2->addWidget(p2);
    QVBoxLayout *v3 = new QVBoxLayout(); v3->addWidget(new QLabel("致命性")); QProgressBar *p3 = new QProgressBar(); p3->setValue(0); p3->setTextVisible(false); v3->addWidget(p3);

    dbLayout->addWidget(lblDNA_Menu);
    dbLayout->addLayout(v1);
    dbLayout->addLayout(v2);
    dbLayout->addLayout(v3);

    diseaseLayout->addWidget(diseaseTopBar);
    diseaseLayout->addWidget(diseaseContent, 1);
    diseaseLayout->addWidget(diseaseBottomBar);

    mainStack->addWidget(diseaseMenu); // Index 1

    // ==========================================
    // 页面 2: 世界概况菜单 (蓝色主题，参考图4、图5)
    // ==========================================
    QFrame *worldMenu = new QFrame();
    worldMenu->setObjectName("WorldMenu");
    QVBoxLayout *worldLayout = new QVBoxLayout(worldMenu);
    worldLayout->setContentsMargins(0, 0, 0, 0);

    // 世界菜单顶部 Tab
    QFrame *worldTopBar = new QFrame();
    worldTopBar->setObjectName("MenuTopBar");
    QHBoxLayout *wtLayout = new QHBoxLayout(worldTopBar);
    QPushButton *btnW1 = new QPushButton("世界概况");
    btnW1->setObjectName("ActiveTabBlue");
    QPushButton *btnW2 = new QPushButton("解药研发");
    QPushButton *btnW3 = new QPushButton("其他数据");
    QPushButton *btnCloseWorld = new QPushButton("✖");
    btnCloseWorld->setObjectName("CloseBtn");

    wtLayout->addWidget(btnW1);
    wtLayout->addWidget(btnW2);
    wtLayout->addWidget(btnW3);
    wtLayout->addStretch();
    wtLayout->addWidget(btnCloseWorld);

    // 世界菜单中间内容 (数据统计占位)
    QWidget *worldContent = new QWidget();
    QVBoxLayout *wcLayout = new QVBoxLayout(worldContent);
    QLabel *worldDataLabel = new QLabel("💙 8,059,506,979\n\n☣ 0\n\n💀 0");
    worldDataLabel->setObjectName("WorldDataLabel");
    wcLayout->addWidget(worldDataLabel);

    worldLayout->addWidget(worldTopBar);
    worldLayout->addWidget(worldContent, 1);

    mainStack->addWidget(worldMenu); // Index 2

    // ==========================================
    // 3. 信号槽绑定与样式应用
    // ==========================================
    setupConnections();
    applyStyle();

    // 页面切换逻辑绑定
    connect(btnDisease, &QPushButton::clicked, [mainStack]() { mainStack->setCurrentIndex(1); });
    connect(btnWorld, &QPushButton::clicked, [mainStack]() { mainStack->setCurrentIndex(2); });
    connect(btnCloseDisease, &QPushButton::clicked, [mainStack]() { mainStack->setCurrentIndex(0); });
    connect(btnCloseWorld, &QPushButton::clicked, [mainStack]() { mainStack->setCurrentIndex(0); });

    // 初始化假数据进行测试
    m_core->initFakeData();
}

MainWindow::~MainWindow()
{
    // delete ui; // 因为没有使用 .ui 文件，所以不需要这行
}

// ==========================================
// C同学需要实现的 UI 更新槽函数
// ==========================================

void MainWindow::updateDayUI(int day)
{
    // 初始日期为 2026年5月31日
    QDate startDate(2026, 5, 31);
    QDate currentDate = startDate.addDays(day);

    // 查找并更新UI中的日期Label
    QLabel *dateLabel = this->findChild<QLabel*>("DateLabel");
    if (dateLabel) {
        dateLabel->setText(currentDate.toString("dd  MM  yyyy"));
    }
}

void MainWindow::updateStatsUI(long totalInfected, long totalDead)
{
    long totalPopulation = 8059506979; // 假定总人口
    long healthy = totalPopulation - totalInfected - totalDead;

    QLabel *worldDataLabel = this->findChild<QLabel*>("WorldDataLabel");
    if (worldDataLabel) {
        QString text = QString("💙 %1\n\n☣ %2\n\n💀 %3")
                           .arg(healthy)
                           .arg(totalInfected)
                           .arg(totalDead);
        worldDataLabel->setText(text);
    }
}

void MainWindow::updateDNAPoints(int points)
{
    // 更新所有叫 DNALabel 的控件 (主界面和疾病菜单中各有一个)
    QList<QLabel*> dnaLabels = this->findChildren<QLabel*>("DNALabel");
    for (QLabel* label : dnaLabels) {
        label->setText(QString("DNA: %1").arg(points));
    }
}

void MainWindow::onStartClicked()
{
    if(m_core) {
        m_core->startGame();
    }
}

// ==========================================
// A同学预留的连接逻辑，由C同学补全
// ==========================================
void MainWindow::setupConnections()
{
    // 核心数据 -> UI 更新
    connect(m_core, &GameCore::dayPassed, this, &MainWindow::updateDayUI);
    connect(m_core, &GameCore::globalStatsUpdated, this, &MainWindow::updateStatsUI);
    connect(m_core, &GameCore::dnaPointsChanged, this, &MainWindow::updateDNAPoints);

    // 核心数据 -> Map 更新 (B同学的方法)
    connect(m_core, &GameCore::countryDataUpdated, m_map, &MapWidget::onCountryDataUpdated);

    // 新闻播报
    connect(m_core, &GameCore::newsGenerated, this, [this](QString text){
        QLabel *newsLabel = this->findChild<QLabel*>("NewsLabel");
        if (newsLabel) {
            newsLabel->setText(text);
        }
    });
}

// ==========================================
// C同学写的 QSS 美化界面 (高度还原截图配色)
// ==========================================
void MainWindow::applyStyle()
{
    this->setStyleSheet(R"(
        /* 整体背景默认黑色 */
        QMainWindow {
            background-color: #000000;
        }

        /* 顶部状态栏和底部状态栏 */
        #TopBar, #BottomBar {
            background-color: rgba(20, 30, 40, 200);
            border-top: 1px solid #444;
            border-bottom: 1px solid #444;
        }

        /* 文字通用颜色 */
        QLabel {
            color: #E0E0E0;
            font-family: "Microsoft YaHei", sans-serif;
            font-weight: bold;
            font-size: 16px;
        }

        /* 日期文字加大 */
        #DateLabel {
            font-size: 20px;
            color: #FFFFFF;
            padding-right: 15px;
        }

        /* 顶部新闻栏 */
        #NewsLabel {
            background-color: #000000;
            border: 1px solid #333;
            border-radius: 5px;
            padding: 5px;
            color: #00FFCC; /* 荧光青色 */
        }

        /* 底部导航大按钮 */
        #MenuBtnRed, #MenuBtnBlue {
            font-size: 18px;
            font-weight: bold;
            padding: 10px 30px;
            border: none;
            color: white;
            border-radius: 2px;
        }
        #MenuBtnRed { background-color: #AA0000; }
        #MenuBtnRed:hover { background-color: #DD0000; }

        #MenuBtnBlue { background-color: #0088CC; }
        #MenuBtnBlue:hover { background-color: #00AAFF; }

        /* DNA 图标样式 */
        #DNALabel {
            color: #AAAAAA;
            font-size: 18px;
            padding-left: 15px;
        }

        /* --- 疾病概况与世界概况 弹出菜单样式 --- */
        #DiseaseMenu {
            background-color: rgba(60, 10, 15, 230); /* 暗红底色 */
        }
        #WorldMenu {
            background-color: rgba(10, 30, 50, 230); /* 暗蓝底色 */
        }

        /* 菜单顶部 Tab 栏 */
        #MenuTopBar {
            background-color: #DDDDDD;
        }
        #MenuTopBar QPushButton {
            background-color: transparent;
            color: #333333;
            font-weight: bold;
            font-size: 16px;
            padding: 15px 30px;
            border: none;
        }
        #MenuTopBar QPushButton:hover {
            background-color: #FFFFFF;
        }

        /* 激活状态的 Tab */
        #ActiveTabRed {
            background-color: #AA0000 !important;
            color: white !important;
        }
        #ActiveTabBlue {
            background-color: #00AADD !important;
            color: white !important;
        }

        /* 关闭按钮 X */
        #CloseBtn {
            color: #AA0000 !important;
            font-size: 24px !important;
            font-weight: 900 !important;
        }

        /* 疾病菜单左侧信息面板 */
        #InfoPanel {
            background-color: #050505;
            color: #AA0000;
            padding: 20px;
            border-right: 2px solid #333;
        }

        /* 疾病菜单底部进度条容器 */
        #DiseaseBottom {
            background-color: #050505;
            border-top: 2px solid #AA0000;
        }

        /* Qt进度条美化 */
        QProgressBar {
            border: 1px solid #333;
            background-color: #222;
            height: 10px;
            border-radius: 2px;
        }
        QProgressBar::chunk {
            background-color: #AA0000; /* 默认红色，可以用不同ObjectName给三种属性分别上色 */
            border-radius: 2px;
        }

        /* 世界数据标签展示 */
        #WorldDataLabel {
            font-size: 24px;
            color: #FFFFFF;
            padding: 40px;
        }
    )");
}
