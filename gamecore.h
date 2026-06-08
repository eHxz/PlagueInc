#ifndef GAMECORE_H
#define GAMECORE_H

#include <QObject>
#include <QTimer>
#include <QVector>
#include <QSet>
#include <QDate>
#include <QString>
#include "GameTypes.h"
#include "skilltree.h"

class GameCore : public QObject
{
    Q_OBJECT
public:
    explicit GameCore(QObject *parent = nullptr);

    // 初始化 10 个地区、人口、接壤关系、技能树（尚未投放病人）
    void initWorld();

    const QVector<RegionData> &regions() const { return m_regions; }
    int regionCount() const { return m_regions.size(); }

    // ---- 供界面读取 ----
    int dnaPoints() const { return m_dnaPoints; }
    DiseaseStats diseaseStats() const { return m_disease; }
    bool isSeeded() const { return m_seeded; }
    bool isDiscovered() const { return m_discovered; }

    // ---- 世界概况 / 其他数据 所需信息 ----
    QString diseaseName() const { return m_diseaseName; }
    int currentDay() const { return m_currentDay; }
    QDate startDate() const { return m_startDate; }
    double cureProgress() const { return m_cureProgress; }
    double cureRatePerDay() const { return m_cureRate; } // 最近一天的解药增量（估算剩余天数）
    long long worldPopulation() const { return m_worldPopulation; }
    long long worldInfected() const;
    long long worldDead() const;
    long long worldAlive() const { return m_worldPopulation - worldDead(); }
    const QVector<HistoryPoint> &history() const { return m_history; }

    const QVector<SkillDef> &skills(int page) const;
    bool isUnlocked(const QString &id) const { return m_unlocked.contains(id); }
    bool isRevealed(const QString &id) const { return m_revealed.contains(id); }
    int  unlockedSkillCount() const { return m_unlocked.size(); } // 已解锁技能数（报告触发用）
    bool canAfford(const QString &id) const;
    bool prereqsMet(const QString &id) const;          // 前置技能是否已全部解锁
    bool isBuyable(const QString &id) const;            // 已显形 + 未解锁 + 前置满足 + 买得起
    QStringList missingPrereqNames(const QString &id) const; // 尚未解锁的前置技能名（用于提示）

public slots:
    void startGame();
    void pauseGame();
    void setSpeed(int intervalMs); // 0 = 暂停；否则按毫秒间隔推进一天
    void collectDNA(int amount);   // 点击气泡收集 DNA

    void seedInfection(int regionId); // 玩家选定初始地区，投放 1 名感染者
    void seedExternalInfection(int regionId); // 受感染的飞机/轮船抵达：在目标地区投放外来病例
    void setDiseaseName(const QString &name); // 初始命名病原体
    void evolveSkill(const QString &id);  // 花费 DNA 解锁技能
    void devolveSkill(const QString &id); // 退化技能：取消加成、退还 DNA（已显示的不再隐藏）

    void setInfiniteDNA(bool on); // 作弊：DNA 无限
    void toggleInfiniteDNA();     // 作弊：F9 一键开/关无限 DNA
    bool infiniteDNA() const { return m_infiniteDna; }

signals:
    void dayPassed(int currentDay);
    void globalStatsUpdated(long long infected, long long dead, long long alive, long long total);
    void regionUpdated(int regionId, RegionData data);     // 数值变化 -> 刷新地图颜色
    void newRegionInfected(int regionId, RegionData data); // 新地区被感染 -> 红色气泡
    void infectionSurge(int regionId, RegionData data);    // 感染激增 -> 橙色气泡（概率）
    void dnaPointsChanged(int currentDNA);
    void cureProgressChanged(double percent);
    void newsGenerated(QString newsText);
    void gameOver(bool humansSurvived, QString message);

    void diseaseStatsChanged(DiseaseStats stats); // 属性变化 -> 刷新条形
    void skillsChanged();                         // 技能解锁/退化 -> 刷新技能树
    void discoveredChanged(bool discovered);      // 疾病被发现
    // 某地区封锁状态变化（formula 形式三）：关闭机场/港口/陆地边境
    void lockdownChanged(int regionId, bool airportClosed, bool portClosed, bool borderClosed);
    // 游戏速度变化（0=暂停，含进入菜单）：供地图让海空载具与游戏时间同步
    void speedChanged(int intervalMs);

private slots:
    void onTick(); // 每天结算

private:
    void emitAllRegions();
    void recomputeDisease();          // 基础值 + 已解锁技能加成
    SkillDef *findSkill(const QString &id);
    void updateLockdowns(long long worldInfected, long long worldAlive); // 形式三：按恐慌指数刷新各地区封锁
    // 公式可调钩子（默认即“未点出相应技能”的状态，便于以后接技能）
    double wealthResistance(int region) const;     // 财富抗性（默认 1.0）
    double landTraitModifier() const;              // 陆地途径乘区（默认 1.0；鸟类/牲畜→更大）
    double borderClosedModifier() const;           // 封锁后的边境修饰（默认 0.0；无视封锁DNA→0.05）
    double airWaterRouteModifier() const;          // 海/空途径乘区（默认极低；空气/水源传播→更大）
    double cureResistance() const;                 // 疾病抗药复杂性（形式五分母；默认 1.0，抗药/基因增强→更大）
    bool   hasAnimalTransmission() const;          // 是否点出【鸟类/家畜传播】（影响陆地乘区与封锁泄漏）

    QTimer *m_timer;
    int m_currentDay;
    int m_dnaPoints;
    double m_dnaAccum;     // DNA 自行缓慢增长的小数累加器
    double m_cureProgress; // 0 ~ 100
    bool m_gameOver;
    bool m_seeded;         // 是否已投放零号病人
    bool m_discovered;     // 疾病是否已被发现（之后才研发疫苗）
    bool m_infiniteDna;    // 作弊：DNA 无限

    DiseaseStats m_disease;      // 有效属性
    double m_baseInfectivity;    // 基础传染性（极低）
    double m_cureRate;           // 最近一天解药增量
    QString m_diseaseName;       // 病原体名称
    QDate m_startDate;           // 开局日期（选定地区时的真实日期）
    QVector<HistoryPoint> m_history; // 历史曲线数据

    QVector<RegionData> m_regions;
    QVector<double> m_infF;  // 精确感染人数（避免极低增速被整数截断）
    QVector<double> m_deadF; // 精确死亡人数
    QVector<quint8> m_airportClosed; // 形式三：各地区机场封锁
    QVector<quint8> m_portClosed;    // 各地区港口封锁
    QVector<quint8> m_borderClosed;  // 各地区陆地边境封锁
    // 各地区独立的三道封锁阈值（resetGame 时按高斯分布随机生成，并升序排列）
    QVector<double> m_lockAirportT;  // 恐慌指数 > 此值 -> 关机场
    QVector<double> m_lockPortT;     //                -> 关港口
    QVector<double> m_lockBorderT;   //                -> 全境封锁
    long long m_worldPopulation;

    QVector<QVector<SkillDef>> m_skills; // 三页技能
    QSet<QString> m_unlocked;            // 已解锁
    QSet<QString> m_revealed;            // 已显示（永久，退化也不隐藏）
};

#endif // GAMECORE_H
