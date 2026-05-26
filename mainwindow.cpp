#include "MainWindow.h"
#include "ui_mainwindow.h"
#include <QVBoxLayout>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    ui->stackedWidget->setCurrentIndex(0);

    // 1. 初始化核心模块
    m_core = new GameCore(this);
    m_map = new MapWidget(this);

    // 2. 界面装配 (TODO: C同学可以在UI Designer里做，也可以在这里写代码)
    // 假设你在 UI Designer 里留了一个叫 mapFrame 的容器
    // QVBoxLayout *layout = new QVBoxLayout(ui->mapFrame);
    // layout->addWidget(m_map);
    QVBoxLayout *layout = new QVBoxLayout(ui->mapFrame);
    layout->addWidget(m_map);
    layout->setContentsMargins(0, 0, 0, 0); // 间距清零

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
    connect(ui->btnStart, &QPushButton::clicked, this, &MainWindow::onStartClicked);

    connect(m_core, &GameCore::globalStatsUpdated, this, &MainWindow::updateStatsUI);

    connect(m_core, &GameCore::dnaPointsChanged, this, &MainWindow::updateDNAPoints);
    // 点击退出按钮，直接关闭整个游戏窗口
    connect(ui->btnExit, &QPushButton::clicked, this, &MainWindow::close);
}

void MainWindow::updateDayUI(int day) {
    // TODO (C同学): ui->lblDay->setText(QString("Day: %1").arg(day));
    ui->lblDay->setText(QString("当前天数: 第 %1 天").arg(day));
}

void MainWindow::updateStatsUI(long totalInfected, long totalDead) {
    // TODO (C同学): 更新界面上的总感染数、总死亡数标签
    ui->lblInfected->setText(QString("全球感染: %1 人").arg(totalInfected));
    ui->lblDead->setText(QString("全球死亡: %1 人").arg(totalDead));
    // 健康人数如何处理之后再说
    //ui->lblHealthy->setText(QString("健康人数: %1 人").arg(totalHealthy));
}

void MainWindow::updateDNAPoints(int points) {
    // TODO (C同学): 更新 DNA 点数显示
    ui->lblDNA->setText(QString("DNA 点数: %1").arg(points));
}

void MainWindow::onStartClicked() {
    m_core->startGame();
    // TODO (C同学): 切换 QStackedWidget 到游戏运行页面
    ui->stackedWidget->setCurrentIndex(1);
}

void MainWindow::applyStyle() {
    // TODO (C同学): 使用 this->setStyleSheet(...) 写 QSS 美化界面
    this->setStyleSheet(
        "QMainWindow, QWidget#centralWidget, QStackedWidget, QWidget { "
        "   background-color: #2c3e50; " //深灰蓝
        "}"

        // 基础文字样式
        "QLabel { "
        "   font-family: 'Microsoft YaHei', sans-serif; "
        "   font-size: 16px; "
        "   font-weight: bold; "
        "}"

        // 纯白
        "QLabel#lblDay { "
        "   color: #ffffff; "
        "}"

        // 粉绿
        "QLabel#lblHealthy { "
        "   color: #2ecc71; "
        "}"

        // 荧光黄
        "QLabel#lblInfected { "
        "   color: #f1c40f; "
        "}"

        // 红
        "QLabel#lblDead { "
        "   color: #e74c3c; "
        "}"
        );
}
