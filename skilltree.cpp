#include "skilltree.h"
#include <QRandomGenerator>
#include <cstdlib>

// ============================================================================
//  技能树配置中心
//  想改“某个技能的消耗 / 加成 / 介绍 / 位置 / 解锁关系”，只改本文件即可。
//
//  位置：每个技能落在 10 列 ×(6/5 交替)=55 格的平顶蜂窝某一格上（cell = 1..55，
//        一列一列从上到下编号；图标取自 graph/六边形/<页>/<cell>.png）。
//  解锁关系：由蜂窝“物理相邻”自动生成（linkByAdjacency）—— 把基础/进化/极端技能
//        放在相邻格，它们就自动互为前置；无需手写 neighbors。
//  数值：暂定，带 ±10% 随机抖动（体现“暂时随机生成”），后续可直接填确定值。
//        与 graph/六边形/技能配置表.txt 对应。
// ============================================================================

namespace {

// 蜂窝列尺寸：偶数列 6 格、奇数列 5 格，合计 55。
const int kColSizes[10] = {6, 5, 6, 5, 6, 5, 6, 5, 6, 5};

// cell(1..55) -> (列, 行)。一列一列、自上而下编号。
bool cellToColRow(int n, int &col, int &row)
{
    int idx = 1;
    for (int c = 0; c < 10; ++c) {
        if (n >= idx && n < idx + kColSizes[c]) {
            col = c;
            row = n - idx;
            return true;
        }
        idx += kColSizes[c];
    }
    return false;
}

// 两个蜂窝格是否物理相邻（平顶六边形：同列上下相邻，或左右相邻列错半格）。
bool cellsAdjacent(int a, int b)
{
    int ca, ra, cb, rb;
    if (!cellToColRow(a, ca, ra) || !cellToColRow(b, cb, rb))
        return false;
    if (ca == cb)
        return std::abs(ra - rb) == 1;          // 同列：上下相邻
    if (std::abs(ca - cb) != 1)
        return false;                           // 非邻列：不相邻
    // 偶数列向上错半格 -> 邻列的 (ra-1, ra) 与之相邻；奇数列向下错半格 -> (ra, ra+1)。
    return (ca % 2 == 0) ? (rb == ra - 1 || rb == ra)
                         : (rb == ra || rb == ra + 1);
}

// 依蜂窝相邻关系，为同一页内所有技能自动补全双向 neighbors。
void linkByAdjacency(QVector<SkillDef> &v)
{
    for (int i = 0; i < v.size(); ++i)
        for (int j = i + 1; j < v.size(); ++j) {
            if (!cellsAdjacent(v[i].cell, v[j].cell))
                continue;
            if (!v[i].neighbors.contains(v[j].id))
                v[i].neighbors.append(v[j].id);
            if (!v[j].neighbors.contains(v[i].id))
                v[j].neighbors.append(v[i].id);
        }
}

// 给某技能设定前置（必须全部解锁才可进化）。
void setPrereqs(QVector<SkillDef> &v, const QString &id, const QStringList &prereqs)
{
    for (SkillDef &s : v)
        if (s.id == id) { s.prereqs = prereqs; return; }
}

// 依“前置关系”为同一页补全双向 neighbors（用于显形传播：解锁前置后，后继技能显形）。
void linkByPrereq(QVector<SkillDef> &v)
{
    for (SkillDef &s : v)
        for (const QString &pr : s.prereqs) {
            if (!s.neighbors.contains(pr))
                s.neighbors.append(pr);
            for (SkillDef &t : v)
                if (t.id == pr && !t.neighbors.contains(s.id))
                    t.neighbors.append(s.id);
        }
}

// 给消耗与加成加一点随机抖动，体现“暂时随便生成”。
double jitter(double base, double ratio = 0.1)
{
    if (base == 0.0)
        return 0.0;
    double d = (QRandomGenerator::global()->generateDouble() * 2.0 - 1.0) * ratio;
    return base * (1.0 + d);
}

int jitterCost(int base)
{
    int d = QRandomGenerator::global()->bounded(-2, 3); // -2 ~ +2
    return qMax(1, base + d);
}

SkillDef mk(const QString &id, const QString &name, const QString &glyph,
            const QString &desc, int cost,
            double dInf, double dSev, double dLet, double dCure,
            int cell, bool root)
{
    SkillDef s;
    s.id = id;
    s.name = name;
    s.glyph = glyph;
    s.desc = desc;
    s.cost = jitterCost(cost);
    s.dInfectivity = jitter(dInf);
    s.dSeverity = jitter(dSev);
    s.dLethality = jitter(dLet);
    s.dCureSlow = jitter(dCure);
    s.cell = cell;
    s.root = root;
    return s;
}

} // namespace

QVector<QVector<SkillDef>> buildSkillTree()
{
    QVector<QVector<SkillDef>> pages(SkillPageCount);

    // ---- 传播途径 ----
    {
        QVector<SkillDef> &p = pages[SkillTransmission];
        p << mk("t_bird", "鸟类传播", "🐦",
                "病原体可感染鸟类，借助候鸟迁徙跨区域扩散，提升传染性。",
                10, 0.04, 0, 0, 0, 1, true);
        p << mk("t_bird2", "鸟类传播进化", "🐦",
                "病原体在禽类体内更稳定，迁徙传播范围进一步扩大。",
                16, 0.06, 0, 0, 0, 7, false);
        p << mk("t_rat", "啮齿动物传播", "🐀",
                "鼠类成为携带者，在人口密集区与港口悄然扩散。",
                10, 0.04, 0, 0, 0, 3, true);
        p << mk("t_rat2", "啮齿动物传播进化", "🐀",
                "啮齿动物种群广泛带毒，城市之间扩散更快。",
                16, 0.06, 0, 0, 0, 8, false);
        p << mk("t_live", "家畜传播", "🐄",
                "病原体借助畜牧贸易在家畜之间传播。",
                11, 0.045, 0, 0, 0, 24, true);
        p << mk("t_live2", "家畜传播进化", "🐄",
                "养殖与运输链全面带毒，传染性显著提升。",
                16, 0.06, 0, 0, 0, 19, false);
        p << mk("t_zoonotic", "极端动物性感染", "🦠",
                "病原体可在几乎所有温血动物间跨物种传播，传染性极高。",
                30, 0.1, 0.02, 0, 0, 13, false);
        p << mk("t_insect", "昆虫传播", "🦟",
                "蚊虫叮咬成为新的传播途径，湿热地区尤为有效。",
                11, 0.05, 0, 0, 0, 6, true);
        p << mk("t_insect2", "昆虫传播进化", "🦟",
                "多种节肢动物携带病原体，传播范围扩大。",
                17, 0.07, 0, 0, 0, 11, false);
        p << mk("t_blood", "血液传播", "💉",
                "病原体可通过血液与体液传播。",
                12, 0.05, 0, 0, 0, 28, true);
        p << mk("t_blood2", "血液传播进化", "💉",
                "极少量血液即可致病，输血与共用器械成为隐患。",
                18, 0.07, 0, 0, 0, 22, false);
        p << mk("t_bloodx", "极端血液性寄生", "🩸",
                "病原体寄生于血细胞内，借虫媒与血液高效扩散。",
                30, 0.1, 0.03, 0, 0, 16, false);
        p << mk("t_air", "空气传播", "💨",
                "病原体依附飞沫与尘埃，在密闭空间与航班上传播力强。",
                12, 0.05, 0, 0, 0, 40, true);
        p << mk("t_air2", "空气传播进化", "💨",
                "气溶胶颗粒更细、悬浮更久，传染性大幅提升。",
                18, 0.08, 0, 0, 0, 46, false);
        p << mk("t_water", "水源传播", "🚰",
                "病原体能在水中存活，污染饮用水源造成大范围感染。",
                12, 0.05, 0, 0, 0, 43, true);
        p << mk("t_water2", "水源传播进化", "🚰",
                "病原体在淡水与海水中均可长期存活。",
                18, 0.08, 0, 0, 0, 48, false);
        p << mk("t_aerosol", "极端生物气凝胶", "🌫",
                "病原体形成稳定生物气凝胶，随空气与水循环全球扩散，传染性达到顶峰。",
                35, 0.12, 0, 0, 0, 47, false);
        // 解锁前置（见 graph/六边形/传播途径/传播途径.txt：每个技能后跟随的位置编号）
        setPrereqs(p, "t_bird2",    {"t_bird"});                       // 7  <- 1
        setPrereqs(p, "t_rat2",     {"t_rat"});                        // 8  <- 3
        setPrereqs(p, "t_live2",    {"t_live"});                       // 19 <- 24
        setPrereqs(p, "t_zoonotic", {"t_bird2", "t_rat2", "t_live2"}); // 13 <- 7 8 19
        setPrereqs(p, "t_insect2",  {"t_insect"});                     // 11 <- 6
        setPrereqs(p, "t_blood2",   {"t_blood"});                      // 22 <- 28
        setPrereqs(p, "t_bloodx",   {"t_insect", "t_blood"});          // 16 <- 6 28
        setPrereqs(p, "t_air2",     {"t_air"});                        // 46 <- 40
        setPrereqs(p, "t_water2",   {"t_water"});                      // 48 <- 43
        setPrereqs(p, "t_aerosol",  {"t_air2", "t_water2"});           // 47 <- 46 48
        linkByPrereq(p);
    }

    // ---- 发病症状 ----
    {
        QVector<SkillDef> &p = pages[SkillSymptom];
        p << mk("s_nausea", "恶心", "🤢",
                "感染者反胃恶心，轻微提升严重性。",
                8, 0.01, 0.03, 0, 0, 1, true);
        p << mk("s_vomit", "呕吐", "🤮",
                "剧烈呕吐既痛苦又成为新的传播媒介。",
                12, 0.02, 0.05, 0, 0, 7, false);
        p << mk("s_diarrhea", "腹泻", "💧",
                "腹泻污染水源与环境，提升传染性与严重性。",
                12, 0.03, 0.05, 0, 0, 8, false);
        p << mk("s_dysentery", "痢疾", "🚽",
                "严重的肠道感染，脱水加重病情。",
                14, 0.02, 0.06, 0, 0, 3, false);
        p << mk("s_insanity", "精神错乱", "🌀",
                "神经系统受损导致行为异常，提升严重性。",
                16, 0, 0.07, 0, 0, 4, false);
        p << mk("s_seizure", "癫痫", "⚡",
                "突发性抽搐，危及生命。",
                16, 0, 0.06, 0.02, 0, 10, false);
        p << mk("s_insomnia", "失眠", "🌙",
                "持续失眠削弱免疫力，提升严重性。",
                10, 0, 0.04, 0, 0, 6, true);
        p << mk("s_paranoia", "偏执", "😨",
                "患者陷入妄想与恐惧，加剧社会恐慌。",
                14, 0, 0.05, 0, 0, 11, false);
        p << mk("s_inflammation", "发炎", "🔥",
                "全身性炎症反应，提升严重性。",
                13, 0, 0.05, 0, 0, 16, false);
        p << mk("s_pulmedema", "肺水肿", "🫁",
                "肺部积液导致呼吸困难，致命性上升。",
                18, 0, 0.06, 0.03, 0, 13, false);
        p << mk("s_pneumonia", "肺炎", "🫁",
                "肺部感染引发呼吸困难，咳嗽进一步传播病原体。",
                16, 0.04, 0.06, 0.02, 0, 18, false);
        p << mk("s_fibrosis", "肺间质纤维化", "🫁",
                "肺组织纤维化造成不可逆损伤，致命性显著提升。",
                22, 0, 0.07, 0.05, 0, 19, false);
        p << mk("s_cough", "咳嗽", "😷",
                "咳嗽将病原体喷入空气，提升传染性与严重性。",
                9, 0.04, 0.02, 0, 0, 23, true);
        p << mk("s_sneeze", "喷嚏", "🤧",
                "喷嚏大量散播飞沫，显著提升传染性。",
                10, 0.05, 0.02, 0, 0, 29, false);
        p << mk("s_immuno", "免疫抑制", "🛡",
                "抑制宿主免疫系统，加重并发症。",
                20, 0, 0.05, 0.04, 0, 30, false);
        p << mk("s_organfail", "全器官衰竭", "🫀",
                "多器官同时衰竭，严重性与致命性大幅上升。",
                28, 0, 0.1, 0.08, 0, 25, false);
        p << mk("s_coma", "昏迷", "💤",
                "感染者陷入昏迷，濒临死亡。",
                26, 0, 0.08, 0.06, 0, 26, false);
        p << mk("s_paralysis", "瘫痪", "🦽",
                "神经损伤导致肢体瘫痪，致命性上升。",
                22, 0, 0.07, 0.04, 0, 21, false);
        p << mk("s_anaphylaxis", "极度过敏", "🐝",
                "全身性过敏反应，可引发休克。",
                22, 0, 0.06, 0.05, 0, 22, false);
        p << mk("s_cyst", "囊肿", "🔵",
                "组织内形成囊肿，破裂后扩散病原体。",
                14, 0.02, 0.05, 0, 0, 28, true);
        p << mk("s_sepsis", "系统性感染", "☣",
                "病原体侵入全身各系统，严重性大幅提升。",
                26, 0, 0.08, 0.06, 0, 32, false);
        p << mk("s_abscess", "脓肿", "🩹",
                "化脓性病灶溃破，成为新的传播源。",
                16, 0.02, 0.06, 0, 0, 33, false);
        p << mk("s_rash", "皮疹", "🔴",
                "皮肤出现红疹，提升严重性。",
                9, 0, 0.04, 0, 0, 45, true);
        p << mk("s_sweat", "出汗", "💦",
                "大量出汗经体液传播病原体。",
                10, 0.04, 0.02, 0, 0, 40, false);
        p << mk("s_fever", "发烧", "🌡",
                "高热削弱宿主，并随汗液与飞沫传播。",
                16, 0.03, 0.05, 0.02, 0, 35, false);
        p << mk("s_dermatosis", "皮肤病", "🩻",
                "皮肤病变伴随脱屑，提升严重性与传播。",
                14, 0, 0.05, 0, 0, 41, false);
        p << mk("s_necrosis", "坏死", "🖤",
                "组织坏死引发剧烈病变，致命性显著提升。",
                26, 0, 0.08, 0.07, 0, 47, false);
        p << mk("s_anemia", "贫血", "🩸",
                "红细胞破坏导致贫血，加重全身损害。",
                16, 0, 0.05, 0.03, 0, 50, true);
        p << mk("s_hemophilia", "血友病", "🩸",
                "凝血功能障碍，轻微创伤即可致命。",
                22, 0, 0.06, 0.05, 0, 44, false);
        p << mk("s_tumor", "肿瘤", "🎗",
                "异常增生的肿瘤压迫器官，严重性与致命性上升。",
                24, 0, 0.07, 0.05, 0, 38, false);
        p << mk("s_hemorrhage", "内出血", "🩸",
                "血管破裂引发内出血，致命性显著提升。",
                24, 0, 0.08, 0.06, 0, 43, false);
        p << mk("s_shock", "失血性休克", "☠",
                "大量失血导致休克，致命性达到顶峰。",
                40, 0, 0.1, 0.12, 0, 48, false);
        linkByAdjacency(p);
    }

    // ---- 特殊能力 ----
    {
        QVector<SkillDef> &p = pages[SkillAbility];
        p << mk("a_cold", "抗寒能力", "❄",
                "增强病原体在寒冷气候下的存活能力。",
                12, 0.02, 0, 0, 0, 1, true);
        p << mk("a_cold2", "抗寒能力进化", "❄",
                "病原体可在极寒环境中长期存活。",
                18, 0.03, 0, 0, 0, 7, false);
        p << mk("a_heat", "抗热能力", "🔥",
                "增强病原体在炎热气候下的存活能力。",
                12, 0.02, 0, 0, 0, 3, true);
        p << mk("a_heat2", "抗热能力进化", "🔥",
                "病原体可在酷热与干旱环境中存活。",
                18, 0.03, 0, 0, 0, 8, false);
        p << mk("a_env", "环境适应", "🌍",
                "病原体适应各种极端气候，全球均可高效传播。",
                24, 0.04, 0, 0, 0, 13, false);
        p << mk("a_drug", "抗药性", "💊",
                "增强病原体对常见药物的抗性，抗药复杂性 +0.4（解药研发分母变大、变慢）。",
                15, 0, 0, 0, 0.4, 5, true);
        p << mk("a_drug2", "抗药性进化", "💊",
                "病原体对广谱抗生素产生抗性，抗药复杂性 +0.6，解药研发进一步变慢。",
                22, 0, 0, 0, 0.6, 10, false);
        p << mk("a_geneboost", "基因增强", "🧬",
                "强化基因结构，小幅提升传染性，并使抗药复杂性 +0.5（拖慢解药研发）。",
                16, 0.02, 0, 0, 0.5, 11, false);
        p << mk("a_geneboost2", "基因增强进化", "🧬",
                "基因高度稳定，解药更难锁定靶点，抗药复杂性 +0.7。",
                22, 0.02, 0, 0, 0.7, 17, false);
        p << mk("a_recomb1", "基因重组1", "🧬",
                "一次性重排基因：点出时立即使当前解药进度倒退 5%~10%（非持续减速）。",
                18, 0, 0, 0, 0.6, 16, false);
        p << mk("a_recomb2", "基因重组2", "🧬",
                "更剧烈的一次性基因重组：点出时立即使当前解药进度倒退 5%~10%。",
                24, 0, 0, 0, 0.9, 22, false);
        p << mk("a_recomb3", "基因重组3", "🧬",
                "基因序列彻底紊乱：点出时立即使当前解药进度倒退 5%~10%，大后期保命核心手段。",
                30, 0, 0, 0, 1.3, 27, false);
        // 解锁前置（见 graph/六边形/特殊能力/特殊能力.txt）
        setPrereqs(p, "a_cold2",      {"a_cold"});            // 7  <- 1
        setPrereqs(p, "a_heat2",      {"a_heat"});            // 8  <- 3
        setPrereqs(p, "a_env",        {"a_cold2", "a_heat2"});// 13 <- 7 8
        setPrereqs(p, "a_drug2",      {"a_drug"});            // 10 <- 5
        setPrereqs(p, "a_geneboost",  {"a_drug"});            // 11 <- 5
        setPrereqs(p, "a_geneboost2", {"a_geneboost"});       // 17 <- 11
        setPrereqs(p, "a_recomb1",    {"a_drug2"});           // 16 <- 10
        setPrereqs(p, "a_recomb2",    {"a_recomb1"});         // 22 <- 16
        setPrereqs(p, "a_recomb3",    {"a_recomb2"});         // 27 <- 22
        linkByPrereq(p);
    }

    return pages;
}
