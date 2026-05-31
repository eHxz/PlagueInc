#include "GameCore.h"
#include <QDebug>

GameCore::GameCore(QObject *parent)
    : QObject(parent)
    , m_currentDay(0)
    , m_dnaPoints(0)
{
    // 初始化定时器
    m_timer = new QTimer(this);

    // 每当定时器到点（timeout），就执行 onTick 结算一天的数值
    connect(m_timer, &QTimer::timeout, this, &GameCore::onTick);
}

void GameCore::initFakeData()
{
    // TODO (A同学):
    // 第一周：先手动造几个国家数据，确保地图能显示。
    // 未来：可以改为读取 JSON 配置文件。

    CountryData china;
    china.name = "China";
    china.totalPopulation = 1400000000;
    china.infectedCount = 0;
    china.deadCount = 0;
    china.isBorderClosed = false;
    m_worldMap.insert("China", china);

    CountryData usa;
    usa.name = "USA";
    usa.totalPopulation = 330000000;
    usa.infectedCount = 0;
    usa.deadCount = 0;
    usa.isBorderClosed = false;
    m_worldMap.insert("USA", usa);

    qDebug() << "A同学：假数据初始化完成，包含国家：" << m_worldMap.keys();
}

void GameCore::startGame()
{
    if (!m_timer->isActive()) {
        m_timer->start(1000); // 1000毫秒 = 1秒游戏内的一天
        qDebug() << "A同学：引擎启动，时间开始流导...";
    }
}

void GameCore::pauseGame()
{
    m_timer->stop();
    qDebug() << "A同学：引擎暂停";
}

void GameCore::collectDNA(int amount)
{
    m_dnaPoints += amount;
    // 通知 C 同学更新界面上的 DNA 数字
    emit dnaPointsChanged(m_dnaPoints);
}

void GameCore::upgradeTrait(QString traitName)
{
    // TODO (A同学):
    // 第二周任务：根据升级的项目，修改病毒的传染率、死亡率等全局系数
    qDebug() << "A同学：处理升级请求：" << traitName;
}

void GameCore::onTick()
{
    // 1. 天数增加
    m_currentDay++;

    // 2. 模拟简单的感染增长（假逻辑，供第一周测试）
    long totalInfected = 0;
    long totalDead = 0;

    // 遍历地图上的所有国家
    QMap<QString, CountryData>::iterator i;
    for (i = m_worldMap.begin(); i != m_worldMap.end(); ++i) {
        CountryData &data = i.value();

        // 假逻辑：如果还没有感染者，第一天给中国 10 个初始感染
        if (data.name == "China" && data.infectedCount == 0) {
            data.infectedCount = 10;
        }

        // 如果已经有感染者，每天增加 20%
        if (data.infectedCount > 0 && data.infectedCount < data.totalPopulation) {
            data.infectedCount *= 1.2;
            // 确保不超总人口
            if (data.infectedCount > data.totalPopulation)
                data.infectedCount = data.totalPopulation;
        }

        // 累计全球数据
        totalInfected += data.infectedCount;
        totalDead += data.deadCount;

        // 【核心操作】发射信号通知 B 同学：这个国家的数据变了，快去刷地图颜色！
        emit countryDataUpdated(i.key(), data);
    }

    // 3. 【核心操作】发射信号通知 C 同学：全球总数据变了，快去改 UI 数字！
    emit dayPassed(m_currentDay);
    emit globalStatsUpdated(totalInfected, totalDead);

    // 4. 随机产生新闻（创意功能）
    if (m_currentDay % 10 == 0) {
        emit newsGenerated(QString("第 %1 天：病毒正在快速传播！").arg(m_currentDay));
    }
}
