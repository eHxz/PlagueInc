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
#include "hudpanel.h"
#include "notice.h"

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
    void showEvent(QShowEvent *event) override;                 // 窗口显示到最终尺寸后再摆放 HUD
    bool eventFilter(QObject *watched, QEvent *event) override; // 监听作弊码输入

private:
    void setupConnections();
    void applyStyle();
    void refreshBottomBar();
    void highlightSpeed(int which); // 0暂停 1正常 2两倍
    void applySpeed(int which);     // 0暂停 1正常 2两倍
    void layoutHud();               // 摆放主页面 HUD 覆盖层（新闻/DNA/解药/世界条/进度条）
    void setHudVisible(bool on);    // 进入/退出菜单时整体隐藏/显示 HUD

    // 核心组件
    GameCore *m_core;
    MapWidget *m_map;
    PopulationBar *m_popBar;
    DiseaseMenu *m_diseaseMenu = nullptr;
    WorldMenu *m_worldMenu = nullptr;
    NoticePopup *m_noticePopup = nullptr;     // 主页面正中央的报告弹窗
    NoticeManager *m_noticeManager = nullptr; // 报告触发逻辑

    QWidget *m_centralWidget = nullptr;
    QStackedLayout *m_mainStack = nullptr;
    QWidget *m_gameView = nullptr;
    QWidget *m_namePage = nullptr;
    QLineEdit *m_nameEdit = nullptr;
    QFrame *m_gameTopBar = nullptr;
    QFrame *m_gameBottomBar = nullptr;

    // 主页面 HUD 覆盖层（绝对定位在游戏视图上，浮在地图之上）
    HudPanel *m_newsPanel = nullptr;    // 左上角：新闻
    HudPanel *m_diseasePanel = nullptr; // 左下角：DNA（点击进疾病概况）
    HudPanel *m_curePanel = nullptr;    // 右下角：解药进度（点击进世界概况）
    HudPanel *m_worldPanel = nullptr;   // 下方正中：全球/观测地区 名字+感染+死亡（点击进世界概况）
    QWidget *m_topRight = nullptr;      // 右上角：日期 + 设置 + 速度按钮
    QLabel *m_dateLabel = nullptr;
    QPushButton *m_settingsBtn = nullptr;
    int m_newsSlot = -1, m_dnaSlot = -1, m_cureSlot = -1;
    int m_wName = -1, m_wInfected = -1, m_wDead = -1;

    int m_currentSpeed = 1;      // 0暂停 1正常 2两倍
    int m_savedSpeed = 1;        // 进入菜单前的流速
    int m_speedBeforeNotice = 1; // 弹出报告前的流速（关闭后恢复）
    QString m_cheatBuf;          // 作弊码输入缓冲

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
