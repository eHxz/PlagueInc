#include "GameCore.h"
#include "regions.h"
#include "gameparams.h"
#include <QRandomGenerator>
#include <QDebug>
#include <cmath>
#include <random>
#include <algorithm>

// =====================================================================
//  技能 → 公式钩子映射（“哪些技能触发哪种效果”，改这里即可调整）
//  数值（乘区大小）在 gameparams.h；这里只管“由哪些技能触发”。
// =====================================================================
namespace {
// 鸟类/家畜（动物）传播：提升【陆地途径乘区】，并让病毒可越过已封锁的边境
//   （动物迁徙/畜牧贸易不受海关约束）。对应 formula 形式一。
static const QStringList kAnimalTransmissionSkills = {
    "t_bird", "t_bird2", "t_rat", "t_rat2", "t_live", "t_live2", "t_zoonotic"};
// 空气/水源传播：提升【海/空途径乘区】（formula 形式二的载毒概率）。
//   带 “进化/极端” 的技能视为更高等级（乘区取 Lv2）。
static const QStringList kAirWaterLv1Skills = {"t_air", "t_water"};
static const QStringList kAirWaterLv2Skills = {"t_air2", "t_water2", "t_aerosol"};
// 抗药复杂性（解药分母）技能：抗药性 / 基因增强。其 dCureSlow 视为“抗药复杂性增量”。
//   （formula 形式五的分母：越大研发越慢。）
static const QStringList kCureResistanceSkills = {
    "a_drug", "a_drug2", "a_geneboost", "a_geneboost2"};
// 基因重组技能：点出时一次性把当前解药进度倒退（不计入抗药复杂性分母）。
static const QStringList kReshuffleSkills = {"a_recomb1", "a_recomb2", "a_recomb3"};
} // namespace

// ===== 可调参数（集中在此便于平衡）=====
static const double kBaseInfectivity   = 0.04;   // 初始传染性：极低
static const double kDnaPerDay         = 0.25;   // DNA 自行缓慢增长（点/天）
static const double kDiscoverInfected  = 1500.0; // 感染人数达到此值 -> 疾病被发现
static const double kDiscoverSeverity  = 0.30;   // 或严重性达到此值 -> 疾病被发现
static const int    kInfiniteDNA       = 999999; // 作弊时的 DNA 数值
static const int    kDnaCap            = 100;    // DNA 上限
static const int    kDevolveRefund     = 2;      // 退化一项技能固定返还的 DNA（并非全额退还）

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
    m_airportClosed.fill(0, m_regions.size());
    m_portClosed.fill(0, m_regions.size());
    m_borderClosed.fill(0, m_regions.size());

    // 各地区独立的三道封锁阈值：高斯分布抽样（均值见 gameparams），再升序排列，
    // 保证“机场最先关、其次港口、最后全境封锁”的收紧顺序；夹到合理区间防极端值。
    m_lockAirportT.resize(m_regions.size());
    m_lockPortT.resize(m_regions.size());
    m_lockBorderT.resize(m_regions.size());
    {
        std::mt19937 rng(QRandomGenerator::global()->generate());
        std::normal_distribution<double> dAir(GameParams::kLockAirportThreshold, GameParams::kLockThresholdSigma);
        std::normal_distribution<double> dPort(GameParams::kLockPortThreshold,   GameParams::kLockThresholdSigma);
        std::normal_distribution<double> dBord(GameParams::kLockBorderThreshold,  GameParams::kLockThresholdSigma);
        for (int i = 0; i < m_regions.size(); ++i) {
            double t[3] = {
                qBound(0.15, dAir(rng),  0.98),
                qBound(0.15, dPort(rng), 0.98),
                qBound(0.15, dBord(rng), 0.98),
            };
            std::sort(t, t + 3);
            m_lockAirportT[i] = t[0];
            m_lockPortT[i]    = t[1];
            m_lockBorderT[i]  = t[2];
        }
    }

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

bool GameCore::prereqsMet(const QString &id) const
{
    for (const QVector<SkillDef> &page : m_skills)
        for (const SkillDef &s : page)
            if (s.id == id) {
                for (const QString &pr : s.prereqs)
                    if (!m_unlocked.contains(pr))
                        return false;
                return true;
            }
    return true;
}

bool GameCore::isBuyable(const QString &id) const
{
    return m_revealed.contains(id) && !m_unlocked.contains(id)
           && prereqsMet(id) && canAfford(id);
}

QStringList GameCore::missingPrereqNames(const QString &id) const
{
    QStringList names;
    const SkillDef *self = nullptr;
    for (const QVector<SkillDef> &page : m_skills)
        for (const SkillDef &s : page)
            if (s.id == id) { self = &s; break; }
    if (!self)
        return names;
    for (const QString &pr : self->prereqs) {
        if (m_unlocked.contains(pr))
            continue;
        for (const QVector<SkillDef> &page : m_skills)
            for (const SkillDef &s : page)
                if (s.id == pr) { names << s.name; break; }
    }
    return names;
}

void GameCore::setInfiniteDNA(bool on)
{
    if (m_infiniteDna == on)
        return;
    m_infiniteDna = on;
    if (on) {
        m_dnaPoints = kInfiniteDNA;
        emit dnaPointsChanged(m_dnaPoints);
        emit newsGenerated("作弊已开启：DNA 无限");
    } else {
        m_dnaPoints = qMin(m_dnaPoints, kDnaCap); // 退出作弊：DNA 回到上限以内
        emit dnaPointsChanged(m_dnaPoints);
        emit newsGenerated("作弊已关闭：DNA 恢复正常");
    }
}

void GameCore::toggleInfiniteDNA()
{
    setInfiniteDNA(!m_infiniteDna);
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
    // 海/空途径乘区随【空气/水源传播】技能提升，随 diseaseStatsChanged 一并下发给地图
    d.routeModifier = airWaterRouteModifier();
    m_disease = d;
}

void GameCore::evolveSkill(const QString &id)
{
    if (m_unlocked.contains(id) || !m_revealed.contains(id))
        return;
    if (!prereqsMet(id))    // 必须先解锁全部前置技能（传播途径/特殊能力）
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

    // 基因重组（formula 形式五）：点出时一次性把当前解药进度倒退 5~10 个百分点。
    if (kReshuffleSkills.contains(id) && m_cureProgress > 0.0) {
        const double setback = GameParams::kReshuffleSetbackMin
            + QRandomGenerator::global()->generateDouble()
              * (GameParams::kReshuffleSetbackMax - GameParams::kReshuffleSetbackMin);
        m_cureProgress = qBound(0.0, m_cureProgress - setback, 100.0);
        emit cureProgressChanged(m_cureProgress);
        emit newsGenerated(QString("【%1】触发基因重组，解药进度倒退 %2%！")
                               .arg(s->name).arg(QString::number(setback, 'f', 1)));
    }

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
        m_dnaPoints = qMin(m_dnaPoints + kDevolveRefund, kDnaCap); // 固定返还 2 点（非全额退还）

    // 级联退化：任何前置已不再满足的已解锁技能也一并退化并固定返还 DNA，避免出现“悬空”解锁。
    bool changed = true;
    while (changed) {
        changed = false;
        for (const QVector<SkillDef> &page : m_skills)
            for (const SkillDef &d : page) {
                if (!m_unlocked.contains(d.id))
                    continue;
                bool met = true;
                for (const QString &pr : d.prereqs)
                    if (!m_unlocked.contains(pr)) { met = false; break; }
                if (!met) {
                    m_unlocked.remove(d.id);
                    if (!m_infiniteDna)
                        m_dnaPoints = qMin(m_dnaPoints + kDevolveRefund, kDnaCap);
                    changed = true;
                }
            }
    }
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
        emit speedChanged(0); // 暂停（含进入菜单）：通知地图冻结海空载具
        return;
    }
    m_timer->start(intervalMs);
    emit speedChanged(intervalMs); // 通知地图按倍速同步载具动画
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

// 受感染的飞机/轮船抵达：在目标地区投放外来病例（formula.md 形式二的落地端）。
// 投放量为 GameParams::kExternalSeedInfected，可在 gameparams.h 调整。
void GameCore::seedExternalInfection(int regionId)
{
    if (!m_seeded || m_gameOver)
        return;
    if (regionId < 0 || regionId >= m_regions.size())
        return;
    RegionData &r = m_regions[regionId];
    if (r.alive() <= 0)
        return; // 该地区已无活人

    const bool wasInfected = r.isInfected;
    const double cap = static_cast<double>(r.alive());
    m_infF[regionId] = qMin(cap, m_infF[regionId] + static_cast<double>(GameParams::kExternalSeedInfected));
    r.isInfected = true;
    r.infectedCount = llround(m_infF[regionId]);

    emit regionUpdated(regionId, r);
    if (!wasInfected) {
        emit newRegionInfected(regionId, r);
        emit newsGenerated(QString("一例输入性病例经海空口岸抵达【%1】！").arg(r.name));
    }
}

void GameCore::emitAllRegions()
{
    for (int i = 0; i < m_regions.size(); ++i)
        emit regionUpdated(i, m_regions[i]);
}

// 公式可调钩子（默认即“未点出相应技能”的状态；接技能时只改这三处）
double GameCore::wealthResistance(int /*region*/) const
{
    // 财富抗性：默认无差别。若想让富国更早封锁，可按 region 返回 <1.0 的值。
    return GameParams::kWealthResistanceDefault;
}
double GameCore::landTraitModifier() const
{
    // 陆地途径乘区：点出【鸟类/家畜传播】后返回 kTraitModifierAnimal（动物随迁徙/贸易跨区），
    // 否则为默认 1.0。
    return hasAnimalTransmission() ? GameParams::kTraitModifierAnimal
                                   : GameParams::kTraitModifierDefault;
}
double GameCore::borderClosedModifier() const
{
    // 边境封锁后的修饰：默认 0.0（完全封死）；点出【鸟类/家畜传播】后，动物仍可携带病毒
    // 突破封锁的边境，返回 kBorderClosedAnimalLeak（小幅泄漏）。
    return hasAnimalTransmission() ? GameParams::kBorderClosedAnimalLeak
                                   : GameParams::kBorderClosed;
}
bool GameCore::hasAnimalTransmission() const
{
    for (const QString &id : kAnimalTransmissionSkills)
        if (m_unlocked.contains(id))
            return true;
    return false;
}
double GameCore::airWaterRouteModifier() const
{
    // 海/空途径乘区（formula 形式二）：默认极低（海关滤网/水净化）；
    // 点出【空气/水源传播】后大幅提升，2 级更高。取已解锁技能中的最高等级。
    double m = GameParams::kRouteModifierBase;
    for (const QString &id : kAirWaterLv1Skills)
        if (m_unlocked.contains(id))
            m = qMax(m, GameParams::kRouteModifierAirWaterLv1);
    for (const QString &id : kAirWaterLv2Skills)
        if (m_unlocked.contains(id))
            m = qMax(m, GameParams::kRouteModifierAirWaterLv2);
    return m;
}

// 疾病抗药复杂性（formula 形式五的分母）：初始 1.0，随【抗药性/基因增强】系列累加，
// 越大则解药研发越慢。基因重组不计入此处（其为一次性倒退进度）。
double GameCore::cureResistance() const
{
    double r = GameParams::kCureResistanceBase;
    for (const SkillDef &s : m_skills[SkillAbility])
        if (m_unlocked.contains(s.id) && kCureResistanceSkills.contains(s.id))
            r += s.dCureSlow;
    return qMax(0.1, r); // 防止除零/负值
}

// 形式三：依各地区恐慌指数刷新封锁状态，仅在状态变化时发信号（并提示新闻）。
//   本国恐慌度 = 本国感染比例 × 严重性
//   全球恐慌度 = 全球感染比例 × 严重性 + 致命性
//   总恐慌指数 = (本国×0.7 + 全球×0.3) × 财富抗性
//   注意：感染比例一律相对“存活人口”（总人口−死亡）。否则随着感染者陆续死亡，
//   感染人数/总人口会回落，已封闭的机场/港口在后期被误判为恐慌缓解而重新开放。
void GameCore::updateLockdowns(long long worldInfected, long long worldAlive)
{
    const double worldRatio = worldAlive > 0
                                  ? static_cast<double>(worldInfected) / static_cast<double>(worldAlive)
                                  : 0.0;
    const double globalPanic = worldRatio * m_disease.severity + m_disease.lethality;

    for (int i = 0; i < m_regions.size(); ++i) {
        const RegionData &r = m_regions[i];
        const long long alive = r.alive();
        const double localRatio = alive > 0
                                      ? static_cast<double>(r.infectedCount) / static_cast<double>(alive)
                                      : 0.0;
        const double localPanic = localRatio * m_disease.severity;
        const double idx = (localPanic * GameParams::kPanicLocalWeight
                            + globalPanic * GameParams::kPanicGlobalWeight)
                           * wealthResistance(i);

        const quint8 air  = idx > m_lockAirportT[i] ? 1 : 0;
        const quint8 port = idx > m_lockPortT[i]    ? 1 : 0;
        const quint8 bord = idx > m_lockBorderT[i]  ? 1 : 0;

        if (air == m_airportClosed[i] && port == m_portClosed[i] && bord == m_borderClosed[i])
            continue; // 状态未变

        // 仅当“收紧”到新的更高等级时播报新闻，避免来回切换刷屏
        if (bord && !m_borderClosed[i])
            emit newsGenerated(QString("【%1】宣布全境封锁！").arg(r.name));
        else if (port && !m_portClosed[i])
            emit newsGenerated(QString("【%1】关闭了港口。").arg(r.name));
        else if (air && !m_airportClosed[i])
            emit newsGenerated(QString("【%1】关闭了机场。").arg(r.name));

        m_airportClosed[i] = air;
        m_portClosed[i] = port;
        m_borderClosed[i] = bord;
        emit lockdownChanged(i, air, port, bord);
    }
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

        // 死亡（formula 形式四）：单日死亡率 = 基础致死倍率 × 致命性；
        //   保底死亡 = 基础保底常数 × 致命性（每国每天的绝对保底，只在有致命性时生效）；
        //   今日新增死亡 = 当前感染人数 × 单日死亡率 + 保底死亡，并以当前感染人数封顶
        //   （基础倍率压低、保底受感染数约束，既防一天死光、又保证有致命性时持续有人死亡）。
        double dailyDeathRate = GameParams::kBaseKillRate * m_disease.lethality;
        double floorDeaths = GameParams::kBaseFloorDeaths * m_disease.lethality;
        double newDeaths = qMin(infected, infected * dailyDeathRate + floorDeaths);
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

    // ---------- 2) 跨区扩散（陆地接壤，formula 形式一）----------
    //   单日陆地突破概率 = 基础边境流量 × 源国感染比例 × 传染性 × 陆地途径乘区 × 边境状态修饰
    //   突破成功 -> 目标地区新增 1~5 个感染者。先累计、后统一结算，避免本日内相互影响。
    QVector<double> addInf(m_regions.size(), 0.0);
    for (int i = 0; i < m_regions.size(); ++i) {
        const RegionData &r = m_regions[i];
        if (!r.isInfected)
            continue;
        // 源国感染比例相对“存活人口”（总人口−死亡）；存活为 0 时跳过，避免除以 0。
        const long long srcAlive = r.alive();
        if (srcAlive <= 0)
            continue;
        const double srcRatio = static_cast<double>(r.infectedCount) / static_cast<double>(srcAlive);
        if (srcRatio <= 0.0)
            continue;
        for (int nb : r.neighbors) {
            if (nb < 0 || nb >= m_regions.size())
                continue;
            // 边境状态修饰：源或目标任一封锁陆地边境即受限（默认完全封死，可由无视封锁DNA放宽）
            const double borderMod = (m_borderClosed[i] || m_borderClosed[nb])
                                         ? borderClosedModifier() : GameParams::kBorderOpen;
            if (borderMod <= 0.0)
                continue;
            const double prob = GameParams::kBaseTraffic * srcRatio * m_disease.infectivity
                                * landTraitModifier() * borderMod;
            if (rng->generateDouble() < prob)
                addInf[nb] += 1.0 + rng->bounded(5); // 1~5 个
        }
    }
    for (int idx = 0; idx < m_regions.size(); ++idx) {
        if (addInf[idx] <= 0.0)
            continue;
        RegionData &r = m_regions[idx];
        const bool wasInfected = r.isInfected;
        const double cap = static_cast<double>(r.alive());
        m_infF[idx] = qMin(cap, m_infF[idx] + addInf[idx]);
        r.isInfected = true;
        r.infectedCount = llround(m_infF[idx]);
        if (!wasInfected) {
            emit newRegionInfected(idx, r);
            emit newsGenerated(QString("病毒已扩散至【%1】！").arg(r.name));
        }
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

    // ---------- 5) 解药研发（formula 形式五）：----------
    //   全球研发优先级 = 全球总恐慌度 × 全球存活比例
    //   单日进度新增量 = (基础研发速率 × 全球研发优先级) ÷ 疾病抗药复杂性
    //   初期恐慌低、优先级低 -> 研发极慢；感染扩大、恐慌升高后逐步加快。
    if (m_discovered && worldInfected > 0 && m_cureProgress < 100.0) {
        const double worldRatio = worldAlive > 0
            ? static_cast<double>(worldInfected) / static_cast<double>(worldAlive) : 0.0;
        // 全球总恐慌度（复用形式三）：全球感染比例×严重性 + 致命性，夹到 0~1。
        const double globalPanic = qBound(0.0,
            worldRatio * m_disease.severity + m_disease.lethality, 1.0);
        const double aliveRatio = m_worldPopulation > 0
            ? static_cast<double>(worldAlive) / static_cast<double>(m_worldPopulation) : 0.0;
        const double priority = globalPanic * aliveRatio;        // 全球研发优先级 0~1
        const double rate = GameParams::kCureBaseRate * priority / cureResistance();

        const double before = m_cureProgress;
        m_cureProgress = qBound(0.0, m_cureProgress + rate, 100.0);
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

    // ---------- 5.7) 政府恐慌与封锁（formula 形式三）----------
    // 在广播之前刷新，使随后 dayPassed 触发的海空发车能用到当日最新封锁状态。
    updateLockdowns(worldInfected, worldAlive);

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
