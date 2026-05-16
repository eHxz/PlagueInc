#ifndef GAMECORE_H
#define GAMECORE_H

#include <QObject>
#include <QTimer>
#include <QMap>
#include "GameTypes.h"

class GameCore : public QObject {
    Q_OBJECT
public:
    explicit GameCore(QObject *parent = nullptr);
    void initFakeData(); // A同学初始化一些假数据用于前期测试

public slots:
    // 接收来自 UI 层的操作指令
    void startGame();
    void pauseGame();
    void collectDNA(int amount);
    void upgradeTrait(QString traitName);

signals:
    // 核心信号：数据变化后广播给 UI 和 Map 层
    void dayPassed(int currentDay);
    void globalStatsUpdated(long totalInfected, long totalDead);
    void countryDataUpdated(QString countryName, CountryData data);
    void dnaPointsChanged(int currentDNA);
    void newsGenerated(QString newsText);

private slots:
    void onTick(); // 定时器触发的回调（每一天的结算逻辑）

private:
    QTimer *m_timer;
    int m_currentDay;
    int m_dnaPoints;
    QMap<QString, CountryData> m_worldMap;
};

#endif // GAMECORE_H
