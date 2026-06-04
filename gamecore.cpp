#include "GameCore.h"
#include "regions.h"
#include <QRandomGenerator>
#include <QDebug>
#include <cmath>

// ===== 可调参数（集中在此便于平衡）=====
static const double kBaseInfectivity   = 0.04;   // 初始传染性：极低
static const double kDnaPerDay         = 0.25;   // DNA 自行缓慢增长（点/天）
static const double kDiscoverInfected  = 1500.0; // 感染人数达到此值 -> 疾病被发现
static const double kDiscoverSeverity  = 0.30;   // 或严重性达到此值 -> 疾病被发现
static const double kCureBase          = 0.55;   // 解药研发基础速率系数
static const double kCureSlowScale     = 0.18;   // 特殊能力减缓解药的换算系数
static const int    kInfiniteDNA       = 999999; // 作弊时的 DNA 数值
static const int    kDnaCap            = 100;    // DNA 上限

GameCore::GameCore(QObject *parent)
    : QObject(parent)
    , m_currentDay(0)
    , m_dnaPoints(0)
    , m_dnaAccum(0.0)
    , m_cureProgress(0.0)
    , m_gameOver(false)
    , m_seeded(false)
    , m_discovered(false)
    , m_infiniteDna(false)
    , m_baseInfectivity(kBaseInfectivity)
    , m_cureRate(0.0)
    , m_diseaseName("Plague")
    , m_startDate(QDate::currentDate())
    , m_worldPopulation(0)
{
    m_timer = new QTimer(this);
    connect(m_timer, &QTimer::timeout, this, &GameCore::onTick);
}

void GameCore::initWorld()
{
    m_regions.clear();
    m_currentDay = 0;
    m_dnaPoints = 0;
    m_dnaAccum = 0.0;
    m_cureProgress = 0.0;
    m_gameOver = false;
    m_seeded = false;
    m_discovered = false;
    m_cureRate = 0.0;
    m_startDate = QDate::currentDate();
    m_history.clear();
    m_unlocked.clear();
    m_revealed.clear();

    // 各地区初始人口来自 regions.h（依据 regions.txt）。
    const QStringList names = Regions::names();
    const QVector<QVector<int>> neighbors = Regions::neighborLists();

    m_worldPopulation = 0;
    for (int i = 0; i < names.size(); ++i) {
        RegionData r;
        r.name = names[i];
        r.totalPopulation = Regions::populationFor(i);
        r.infectedCount = 0;
        r.deadCount = 0;
        r.isInfected = false;
        if (i < neighbors.size())
            r.neighbors = neighbors[i];
        m_regions.append(r);
        m_worldPopulation += r.totalPopulation;
    }
    m_infF.fill(0.0, m_regions.size());
    m_deadF.fill(0.0, m_regions.size());

    // 技能树（数值占位，集中在 skilltree.cpp）
    m_skills = buildSkillTree();
    for (const QVector<SkillDef> &page : m_skills)
        for (const SkillDef &s : page)
            if (s.root)
                m_revealed.insert(s.id);

    // 初始属性：传染性极低，致病性与严重性为 0
    m_baseInfectivity = kBaseInfectivity;
    recomputeDisease();

    qDebug() << "世界初始化完成：地区数" << m_regions.size()
             << " 基础传染性" << m_baseInfectivity;

    emitAllRegions();
    emit dnaPointsChanged(m_dnaPoints);
    emit cureProgressChanged(m_cureProgress);
    emit diseaseStatsChanged(m_disease);
    emit discoveredChanged(m_discovered);
    emit skillsChanged();
}

const QVector<SkillDef> &GameCore::skills(int page) const
{
    static const QVector<SkillDef> empty;
    if (page < 0 || page >= m_skills.size())
        return empty;
    return m_skills[page];
}

SkillDef *GameCore::findSkill(const QString &id)
{
    for (QVector<SkillDef> &page : m_skills)
        for (SkillDef &s : page)
            if (s.id == id)
                return &s;
    return nullptr;
}

bool GameCore::canAfford(const QString &id) const
{
    if (m_infiniteDna)
        return true;
    for (const QVector<SkillDef> &page : m_skills)
        for (const SkillDef &s : page)
            if (s.id == id)
                return m_dnaPoints >= s.cost;
    return false;
}

void GameCore::setInfiniteDNA(bool on)
{
    m_infiniteDna = on;
    if (on) {
        m_dnaPoints = kInfiniteDNA;
        emit dnaPointsChanged(m_dnaPoints);
        emit newsGenerated("作弊已开启：DNA 无限");
    }
}

void GameCore::recomputeDisease()
{
    DiseaseStats d;
    d.infectivity = m_baseInfectivity;
    d.severity = 0.0;
    d.lethality = 0.0;
    for (const QVector<SkillDef> &page : m_skills) {
        for (const SkillDef &s : page) {
            if (!m_unlocked.contains(s.id))
                continue;
            d.infectivity += s.dInfectivity;
            d.severity += s.dSeverity;
            d.lethality += s.dLethality;
        }
    }
    d.infectivity = qBound(0.0, d.infectivity, 1.5);
    d.severity = qBound(0.0, d.severity, 1.0);
    d.lethality = qBound(0.0, d.lethality, 0.6);
    m_disease = d;
}

void GameCore::evolveSkill(const QString &id)
{
    if (m_unlocked.contains(id) || !m_revealed.contains(id))
        return;
    SkillDef *s = findSkill(id);
    if (!s || (!m_infiniteDna && m_dnaPoints < s->cost))
        return;

    if (!m_infiniteDna)
        m_dnaPoints -= s->cost;
    m_unlocked.insert(id);
    // 解锁后显示相邻技能（永久可见）
    for (const QString &nb : s->neighbors)
        m_revealed.insert(nb);

    recomputeDisease();
    emit dnaPointsChanged(m_dnaPoints);
    emit diseaseStatsChanged(m_disease);
    emit skillsChanged();
    emit newsGenerated(QString("已进化：%1").arg(s->name));
}

void GameCore::devolveSkill(const QString &id)
{
    if (!m_unlocked.contains(id))
        return;
    SkillDef *s = findSkill(id);
    if (!s)
        return;

    m_unlocked.remove(id);
    if (!m_infiniteDna)
        m_dnaPoints = qMin(m_dnaPoints + s->cost, kDnaCap); // 退还 DNA（不超过上限）
    recomputeDisease();
    emit dnaPointsChanged(m_dnaPoints);
    emit diseaseStatsChanged(m_disease);
    emit skillsChanged();
}

void GameCore::startGame()
{
    if (m_gameOver)
        return;
    if (!m_timer->isActive())
        m_timer->start(m_timer->interval() > 0 ? m_timer->interval() : 1000);
}

void GameCore::pauseGame()
{
    m_timer->stop();
}

void GameCore::setSpeed(int intervalMs)
{
    if (m_gameOver)
        return;
    if (intervalMs <= 0) {
        m_timer->stop();
        return;
    }
    m_timer->start(intervalMs);
}

void GameCore::collectDNA(int amount)
{
    if (m_infiniteDna)
        return;
    m_dnaPoints = qMin(m_dnaPoints + amount, kDnaCap); // 不超过 DNA 上限
    emit dnaPointsChanged(m_dnaPoints);
}

void GameCore::setDiseaseName(const QString &name)
{
    const QString trimmed = name.trimmed();
    m_diseaseName = trimmed.isEmpty() ? QStringLiteral("Plague") : trimmed;
}

long long GameCore::worldInfected() const
{
    long long n = 0;
    for (const RegionData &r : m_regions)
        n += r.infectedCount;
    return n;
}

long long GameCore::worldDead() const
{
    long long n = 0;
    for (const RegionData &r : m_regions)
        n += r.deadCount;
    return n;
}

void GameCore::seedInfection(int regionId)
{
    if (m_seeded || m_gameOver)
        return;
    if (regionId < 0 || regionId >= m_regions.size())
        return;

    m_seeded = true;
    m_currentDay = 0;
    m_startDate = QDate::currentDate(); // 时间从选定地区的真实日期开始
    RegionData &r = m_regions[regionId];
    r.isInfected = true;
    m_infF[regionId] = 1.0; // 从 1 名感染者开始
    r.infectedCount = 1;

    // 记录开局（第 0 天）历史点
    m_history.clear();
    HistoryPoint hp;
    hp.day = 0;
    hp.healthyFrac = 1.0;
    hp.infectedFrac = 0.0;
    hp.deadFrac = 0.0;
    hp.cure = 0.0;
    hp.infectivity = m_disease.infectivity;
    hp.severity = m_disease.severity;
    hp.lethality = m_disease.lethality;
    m_history.append(hp);

    emit regionUpdated(regionId, r);
    emit newRegionInfected(regionId, r);
    emit dayPassed(m_currentDay);
    emit newsGenerated(QString("零号病人出现在【%1】，疫情开始扩散……").arg(r.name));
}

void GameCore::emitAllRegions()
{
    for (int i = 0; i < m_regions.size(); ++i)
        emit regionUpdated(i, m_regions[i]);
}

void GameCore::onTick()
{
    if (m_gameOver || !m_seeded) // 选定地区前时间不流动
        return;

    m_currentDay++;
    auto *rng = QRandomGenerator::global();

    // ---------- DNA 自行缓慢增长（投放后开始）----------
    if (!m_infiniteDna) {
        m_dnaAccum += kDnaPerDay;
        if (m_dnaAccum >= 1.0) {
            int add = static_cast<int>(m_dnaAccum);
            m_dnaAccum -= add;
            m_dnaPoints = qMin(m_dnaPoints + add, kDnaCap); // 受 DNA 上限约束
            emit dnaPointsChanged(m_dnaPoints);
        }
    }

    // ---------- 1) 地区内部增长 + 死亡（解药成功前不会自行康复）----------
    for (int i = 0; i < m_regions.size(); ++i) {
        RegionData &r = m_regions[i];
        if (!r.isInfected)
            continue;

        double total = static_cast<double>(r.totalPopulation);
        double infected = m_infF[i];
        double dead = m_deadF[i];
        double susceptible = total - infected - dead;
        if (susceptible < 0)
            susceptible = 0;

        // 内部增长：与传染性、当前感染数、剩余健康人口占比相关（logistic）
        double newInfections = m_disease.infectivity * infected * (susceptible / total);
        infected += newInfections;
        if (infected > total - dead)
            infected = total - dead;

        // 死亡：仅在有致命性时发生
        double newDeaths = m_disease.lethality * infected;
        infected -= newDeaths;
        dead += newDeaths;
        if (infected < 0)
            infected = 0;
        if (dead > total)
            dead = total;

        m_infF[i] = infected;
        m_deadF[i] = dead;
        r.infectedCount = llround(infected);
        r.deadCount = llround(dead);

        // 感染激增：有概率冒出橙色气泡
        if (newInfections > total * 0.0008 && rng->generateDouble() < 0.05)
            emit infectionSurge(i, r);
    }

    // ---------- 2) 跨区扩散（仅相邻地区）----------
    QVector<int> toInfect;
    for (int i = 0; i < m_regions.size(); ++i) {
        const RegionData &r = m_regions[i];
        if (!r.isInfected)
            continue;
        double ratio = static_cast<double>(r.infectedCount) / static_cast<double>(r.totalPopulation);
        if (ratio < 0.005) // 感染率达到 0.5% 才有较强外溢能力
            continue;
        for (int nb : r.neighbors) {
            if (nb < 0 || nb >= m_regions.size())
                continue;
            if (m_regions[nb].isInfected || toInfect.contains(nb))
                continue;
            double prob = 0.02 + m_disease.infectivity * 0.25;
            if (rng->generateDouble() < prob)
                toInfect.append(nb);
        }
    }
    for (int idx : toInfect) {
        RegionData &r = m_regions[idx];
        r.isInfected = true;
        m_infF[idx] = qMax(m_infF[idx], 10.0);
        r.infectedCount = llround(m_infF[idx]);
        emit newRegionInfected(idx, r);
        emit newsGenerated(QString("病毒已扩散至【%1】！").arg(r.name));
    }

    // ---------- 3) 全球汇总 ----------
    long long worldInfected = 0, worldDead = 0;
    for (const RegionData &r : m_regions) {
        worldInfected += r.infectedCount;
        worldDead += r.deadCount;
    }
    long long worldAlive = m_worldPopulation - worldDead;

    // ---------- 4) 疾病被发现 -> 之后才开始疫苗研究 ----------
    if (!m_discovered && m_seeded
        && (static_cast<double>(worldInfected) >= kDiscoverInfected
            || m_disease.severity >= kDiscoverSeverity)) {
        m_discovered = true;
        emit discoveredChanged(true);
        emit newsGenerated("疾病已被发现，各国紧急启动疫苗研究！");
    }

    // ---------- 5) 疫苗研发：初期缓慢，随感染与严重性加快，受死亡人数制约 ----------
    if (m_discovered && worldInfected > 0 && m_cureProgress < 100.0) {
        double infectedRatio = static_cast<double>(worldInfected) / static_cast<double>(m_worldPopulation);
        double aliveRatio = static_cast<double>(worldAlive) / static_cast<double>(m_worldPopulation);

        // 减缓 / 倒退解药的特殊能力加成
        double cureSlow = 0.0;
        for (const SkillDef &s : m_skills[SkillAbility])
            if (m_unlocked.contains(s.id))
                cureSlow += s.dCureSlow;

        double rate = kCureBase
                      * (0.05 + 4.0 * infectedRatio)   // 感染越广，研究投入越大
                      * (0.40 + 1.2 * m_disease.severity) // 越严重越受重视
                      * aliveRatio;                    // 死亡越多，研究力量越弱
        rate -= cureSlow * kCureSlowScale;             // 可使其倒退

        double before = m_cureProgress;
        m_cureProgress += rate;
        m_cureProgress = qBound(0.0, m_cureProgress, 100.0);
        m_cureRate = m_cureProgress - before; // 实际每天增量（供估算剩余天数）
        emit cureProgressChanged(m_cureProgress);
    } else {
        m_cureRate = 0.0;
    }

    // ---------- 5.5) 记录历史曲线 ----------
    {
        HistoryPoint hp;
        hp.day = m_currentDay;
        double total = static_cast<double>(m_worldPopulation);
        hp.deadFrac = total > 0 ? worldDead / total : 0.0;
        hp.infectedFrac = total > 0 ? worldInfected / total : 0.0;
        hp.healthyFrac = qBound(0.0, 1.0 - hp.deadFrac - hp.infectedFrac, 1.0);
        hp.cure = m_cureProgress;
        hp.infectivity = m_disease.infectivity;
        hp.severity = m_disease.severity;
        hp.lethality = m_disease.lethality;
        m_history.append(hp);
    }

    // ---------- 6) 广播 ----------
    emitAllRegions();
    emit dayPassed(m_currentDay);
    emit globalStatsUpdated(worldInfected, worldDead, worldAlive, m_worldPopulation);

    if (m_currentDay % 15 == 0)
        emit newsGenerated(QString("第 %1 天：全球感染 %2 人").arg(m_currentDay).arg(worldInfected));

    // ---------- 7) 胜负判定 ----------
    if (m_cureProgress >= 100.0) {
        m_gameOver = true;
        m_timer->stop();
        emit gameOver(true, QString("人类研发出解药！病毒被遏制。\n存活 %1 人，死亡 %2 人。")
                                .arg(worldAlive).arg(worldDead));
    } else if (worldAlive <= 0) {
        m_gameOver = true;
        m_timer->stop();
        emit gameOver(false, QString("全人类已灭绝，病毒获胜！\n历时 %1 天。").arg(m_currentDay));
    }
}
