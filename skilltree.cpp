#include "skilltree.h"
#include <QRandomGenerator>

// ============================================================================
//  技能树配置中心
//  想改“某个技能的消耗 / 加成 / 介绍 / 位置 / 相邻关系”，只改本文件即可。
//  目前数值为占位（带 ±10% 随机抖动），后续可直接填确定值。
// ============================================================================

namespace {

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

// 把相邻关系做成双向（解锁任一端都能让另一端显形）。
void linkBoth(QVector<SkillDef> &v)
{
    auto find = [&](const QString &id) -> SkillDef * {
        for (SkillDef &s : v)
            if (s.id == id)
                return &s;
        return nullptr;
    };
    // 收集原始声明的边，统一补全反向
    QVector<QPair<QString, QString>> edges;
    for (const SkillDef &s : v)
        for (const QString &nb : s.neighbors)
            edges.append({s.id, nb});
    for (const auto &e : edges) {
        if (SkillDef *a = find(e.first))
            if (!a->neighbors.contains(e.second))
                a->neighbors.append(e.second);
        if (SkillDef *b = find(e.second))
            if (!b->neighbors.contains(e.first))
                b->neighbors.append(e.first);
    }
}

SkillDef mk(const QString &id, const QString &name, const QString &glyph,
            const QString &desc, int cost,
            double dInf, double dSev, double dLet, double dCure,
            double x, double y, const QStringList &nb, bool root)
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
    s.pos = QPointF(x, y);
    s.neighbors = nb;
    s.root = root;
    return s;
}

} // namespace

QVector<QVector<SkillDef>> buildSkillTree()
{
    QVector<QVector<SkillDef>> pages(SkillPageCount);

    // ----------------------------------------------------------------------
    //  传播途径：提升传染性，部分提供额外扩散能力。
    // ----------------------------------------------------------------------
    {
        QVector<SkillDef> &p = pages[SkillTransmission];
        p << mk("t_bird",  "禽类传播",   "🐦",
                "病原体能够感染鸟类，并随鸟类迁徙跨区域传播，显著提升传染性。",
                10, 0.04, 0, 0, 0, 0.04, 0.18, {"t_bird2"}, true);
        p << mk("t_bird2", "禽类传播 2级", "🐦",
                "变异使病原体在禽类体内更稳定，进一步增强传染性。",
                16, 0.06, 0, 0, 0, 0.16, 0.10, {}, false);
        p << mk("t_live",  "家畜传播",   "🐑",
                "病原体可在家畜间传播，借助畜牧贸易扩散。",
                10, 0.04, 0, 0, 0, 0.30, 0.30, {}, true);
        p << mk("t_rat",   "啮齿动物传播", "🐀",
                "鼠类成为携带者，在人口密集区悄然扩散。",
                10, 0.04, 0, 0, 0, 0.04, 0.45, {}, true);
        p << mk("t_insect","昆虫传播",   "🦟",
                "蚊虫叮咬成为新的传播途径，在湿热地区尤为有效。",
                11, 0.045, 0, 0, 0, 0.05, 0.74, {}, true);
        p << mk("t_blood", "血液传播",   "💉",
                "病原体可通过血液与体液传播，提升传染性。",
                13, 0.05, 0, 0, 0, 0.30, 0.78, {}, true);
        p << mk("t_water", "水源传播",   "🚰",
                "病原体能在水中存活，污染饮用水源造成大范围感染。",
                12, 0.05, 0, 0, 0, 0.50, 0.62, {"t_water2"}, true);
        p << mk("t_water2","水源传播 2级", "🚰",
                "病原体在淡水与海水中均可长期存活，传染性进一步提升。",
                18, 0.07, 0, 0, 0, 0.60, 0.70, {}, false);
        p << mk("t_air",   "空气传播",   "💨",
                "病原体能依附在空气尘埃上，尤其在干燥环境和飞机上传播力极强。",
                12, 0.05, 0, 0, 0, 0.50, 0.28, {"t_air2"}, true);
        p << mk("t_air2",  "空气传播 2级", "💨",
                "气溶胶颗粒更细，悬浮时间更久，传染性大幅提升。",
                18, 0.07, 0, 0, 0, 0.60, 0.18, {}, false);
        linkBoth(p);
    }

    // ----------------------------------------------------------------------
    //  发病症状：提升严重性与致命性，部分附带传染性。
    // ----------------------------------------------------------------------
    {
        QVector<SkillDef> &p = pages[SkillSymptom];
        p << mk("s_nausea", "恶心", "🤢",
                "感染者出现恶心反胃，轻微提升严重性。",
                8, 0.01, 0.03, 0, 0, 0.04, 0.20, {"s_vomit", "s_insomnia"}, true);
        p << mk("s_insomnia", "失眠", "🌙",
                "持续失眠削弱免疫力，提升严重性。",
                10, 0, 0.04, 0, 0, 0.04, 0.55, {}, false);
        p << mk("s_vomit", "呕吐", "🤮",
                "剧烈呕吐既痛苦又成为新的传播媒介。",
                12, 0.02, 0.05, 0, 0, 0.16, 0.40, {"s_organfail"}, false);
        p << mk("s_cough", "咳嗽", "😷",
                "咳嗽将病原体喷入空气，提升传染性与严重性。",
                9, 0.04, 0.02, 0, 0, 0.28, 0.16, {"s_pneumonia"}, true);
        p << mk("s_pneumonia", "肺炎", "🫁",
                "肺部感染导致呼吸困难，严重性与致命性明显上升。",
                16, 0.05, 0.06, 0.02, 0, 0.30, 0.42, {"s_organfail"}, false);
        p << mk("s_rash", "皮疹", "🔴",
                "皮肤出现红疹，提升严重性。",
                9, 0, 0.04, 0, 0, 0.52, 0.16, {"s_pustule"}, true);
        p << mk("s_pustule", "脓肿", "🩹",
                "受感染的皮肤组织溃烂成为病原体的繁殖区，伤口破裂可提升传染性。",
                14, 0.02, 0.06, 0, 0, 0.52, 0.42, {"s_hemorrhage"}, false);
        p << mk("s_hemorrhage", "内出血", "🩸",
                "血管破裂引发内出血，致命性显著提升。",
                20, 0, 0.08, 0.05, 0, 0.42, 0.62, {"s_organfail"}, false);
        p << mk("s_organfail", "器官衰竭", "🫀",
                "多器官功能衰竭，严重性与致命性大幅上升。",
                28, 0, 0.10, 0.08, 0, 0.30, 0.74, {"s_coma"}, false);
        p << mk("s_coma", "昏迷", "💤",
                "感染者陷入昏迷，濒临死亡。",
                30, 0, 0.08, 0.06, 0, 0.16, 0.78, {"s_total"}, false);
        p << mk("s_total", "全身性休克", "☠",
                "全身性休克导致迅速死亡，致命性达到顶峰。",
                40, 0, 0.12, 0.12, 0, 0.05, 0.82, {}, false);
        linkBoth(p);
    }

    // ----------------------------------------------------------------------
    //  特殊能力：只保留最左侧两大类（环境抗性 / 药物抗性）+ 减缓解药的基因。
    // ----------------------------------------------------------------------
    {
        QVector<SkillDef> &p = pages[SkillAbility];
        // 环境抗性
        p << mk("a_cold", "寒冷抗性 1级", "❄",
                "增强病原体在寒冷气候下的存活能力。",
                12, 0.02, 0, 0, 0, 0.04, 0.16, {"a_cold2"}, true);
        p << mk("a_cold2", "寒冷抗性 2级", "❄",
                "病原体可在极寒环境中长期存活。",
                18, 0.03, 0, 0, 0, 0.16, 0.10, {}, false);
        p << mk("a_heat", "高温抗性 1级", "🔥",
                "增强病原体在炎热气候下的存活能力。",
                12, 0.02, 0, 0, 0, 0.04, 0.44, {"a_heat2"}, true);
        p << mk("a_heat2", "高温抗性 2级", "🔥",
                "病原体可在酷热与干旱环境中存活。",
                18, 0.03, 0, 0, 0, 0.16, 0.50, {}, false);
        // 药物抗性
        p << mk("a_drug", "药物抗性 1级", "💊",
                "增强病原体对常见药物的抗性，轻微减缓解药研发。",
                15, 0, 0, 0, 0.4, 0.04, 0.72, {"a_drug2"}, true);
        p << mk("a_drug2", "药物抗性 2级", "💊",
                "病原体对广谱抗生素产生抗性，进一步减缓解药研发。",
                22, 0, 0, 0, 0.6, 0.16, 0.78, {}, false);
        // 减缓 / 倒退解药研发的基因
        p << mk("a_gene1", "基因重组 1级", "🧬",
                "频繁的基因重组使解药难以锁定靶点，减缓其研发进度。",
                14, 0, 0, 0, 0.5, 0.34, 0.30, {"a_gene2"}, true);
        p << mk("a_gene2", "基因硬化 2级", "🧬",
                "强化基因结构，进一步拖慢解药研发。",
                20, 0, 0, 0, 0.7, 0.46, 0.24, {}, false);
        p << mk("a_supp1", "抑制修饰基因 1级", "🧪",
                "撤销关键基因以干扰解药团队的分析，减缓其研发。",
                18, 0, 0, 0, 0.8, 0.34, 0.56, {"a_supp2"}, true);
        p << mk("a_supp2", "抑制修饰基因 2级", "🧪",
                "彻底打乱基因序列，可使解药研发进度不增反退。",
                26, 0, 0, 0, 1.3, 0.46, 0.60, {}, false);
        linkBoth(p);
    }

    return pages;
}
