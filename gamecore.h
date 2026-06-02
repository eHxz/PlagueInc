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
    bool canAfford(const QString &id) const;

public slots:
    void startGame();
    void pauseGame();
    void setSpeed(int intervalMs); // 0 = 暂停；否则按毫秒间隔推进一天
    void collectDNA(int amount);   // 点击气泡收集 DNA

    void seedInfection(int regionId); // 玩家选定初始地区，投放 1 名感染者
    void evolveSkill(const QString &id);  // 花费 DNA 解锁技能
    void devolveSkill(const QString &id); // 退化技能：取消加成、退还 DNA（已显示的不再隐藏）

    void setInfiniteDNA(bool on); // 作弊：DNA 无限

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

private slots:
    void onTick(); // 每天结算

private:
    void emitAllRegions();
    void recomputeDisease();          // 基础值 + 已解锁技能加成
    SkillDef *findSkill(const QString &id);

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
    long long m_worldPopulation;

    QVector<QVector<SkillDef>> m_skills; // 三页技能
    QSet<QString> m_unlocked;            // 已解锁
    QSet<QString> m_revealed;            // 已显示（永久，退化也不隐藏）
};

#endif // GAMECORE_H
