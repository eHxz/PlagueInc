#include "MainWindow.h"
#include "ui_mainwindow.h"
#include <QVBoxLayout>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    // 1. 初始化核心模块
    m_core = new GameCore(this);
    m_map = new MapWidget(this);

    // 2. 界面装配 (TODO: C同学可以在UI Designer里做，也可以在这里写代码)
    // 假设你在 UI Designer 里留了一个叫 mapFrame 的容器
    // QVBoxLayout *layout = new QVBoxLayout(ui->mapFrame);
    // layout->addWidget(m_map);

    m_core->initFakeData(); // 初始化假数据
    setupConnections();
    applyStyle();
}

MainWindow::~MainWindow() {
    delete ui;
}

void MainWindow::setupConnections() {
    // TODO (C同学): 在这里连接 A、B、C 之间的信号与槽

    // 示例：当 Core 发射天数变化信号，更新 UI 上的天数标签
    connect(m_core, &GameCore::dayPassed, this, &MainWindow::updateDayUI);

    // 示例：当数据变化，更新地图颜色（B同学的功能）
    connect(m_core, &GameCore::countryDataUpdated, m_map, &MapWidget::onCountryDataUpdated);

    // 示例：点击开始按钮，启动游戏
    // connect(ui->btnStart, &QPushButton::clicked, this, &MainWindow::onStartClicked);
}

void MainWindow::updateDayUI(int day) {
    // TODO (C同学): ui->lblDay->setText(QString("Day: %1").arg(day));
}

void MainWindow::updateStatsUI(long totalInfected, long totalDead) {
    // TODO (C同学): 更新界面上的总感染数、总死亡数标签
}

void MainWindow::updateDNAPoints(int points) {
    // TODO (C同学): 更新 DNA 点数显示
}

void MainWindow::onStartClicked() {
    m_core->startGame();
    // TODO (C同学): 切换 QStackedWidget 到游戏运行页面
}

void MainWindow::applyStyle() {
    // TODO (C同学): 使用 this->setStyleSheet(...) 写 QSS 美化界面
}
