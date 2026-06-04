#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QLabel>
#include <QMainWindow>
#include <QFrame>
#include <QProgressBar>
#include <QPushButton>
#include "GameCore.h"
#include "MapWidget.h"
#include "populationbar.h"
#include "diseasemenu.h"
#include "worldmenu.h"

class QLineEdit;
class QStackedLayout;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    // 引擎 -> 界面
    void updateDayUI(int day);
    void updateStatsUI(long long infected, long long dead, long long alive, long long total);
    void updateDNAPoints(int points);
    void updateCure(double percent);
    void onRegionUpdated(int regionId, RegionData data);
    void onGameOver(bool humansSurvived, QString message);

    // 地图点击
    void onRegionSelected(int regionId);
    void onOceanClicked();
    void onStartRequested(int regionId); // 点击开始气泡 -> 投放零号病人

    // 速度按钮
    void setSpeedPaused();
    void setSpeedNormal();
    void setSpeedFast();

    // 疾病菜单（半透明覆盖层）
    void openDiseaseMenu();
    void closeDiseaseMenu();

    // 世界概况菜单（半透明覆盖层）
    void openWorldMenu();
    void closeWorldMenu();

    // 初始命名页 / 设置
    void confirmDiseaseName(); // 命名后进入主游戏
    void openSettings();       // 左上角设置：返回 / 重开
    void restartGame();        // 重开：回到命名页

protected:
    void resizeEvent(QResizeEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override; // 监听作弊码输入

private:
    void setupConnections();
    void applyStyle();
    void refreshBottomBar();
    void highlightSpeed(int which); // 0暂停 1正常 2两倍
    void applySpeed(int which);     // 0暂停 1正常 2两倍

    // 核心组件
    GameCore *m_core;
    MapWidget *m_map;
    PopulationBar *m_popBar;
    DiseaseMenu *m_diseaseMenu = nullptr;
    WorldMenu *m_worldMenu = nullptr;

    QWidget *m_centralWidget = nullptr;
    QStackedLayout *m_mainStack = nullptr;
    QWidget *m_gameView = nullptr;
    QWidget *m_namePage = nullptr;
    QLineEdit *m_nameEdit = nullptr;
    QFrame *m_gameTopBar = nullptr;
    QFrame *m_gameBottomBar = nullptr;
    int m_currentSpeed = 1; // 0暂停 1正常 2两倍
    int m_savedSpeed = 1;   // 进入菜单前的流速
    QString m_cheatBuf;     // 作弊码输入缓冲

    // 速度按钮
    QPushButton *m_btnPause = nullptr;
    QPushButton *m_btnNormal = nullptr;
    QPushButton *m_btnFast = nullptr;

    // 当前底部条显示的对象：-1 = 全球，否则地区下标
    int m_selectedRegion = -1;

    // 最近一次的世界与各地区数据（用于刷新底部条）
    long long m_worldInfected = 0;
    long long m_worldDead = 0;
    long long m_worldAlive = 0;
    long long m_worldTotal = 0;
    QVector<RegionData> m_regionSnapshot;
};

#endif // MAINWINDOW_H
