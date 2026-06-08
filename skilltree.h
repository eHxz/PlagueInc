#ifndef SKILLTREE_H
#define SKILLTREE_H

#include <QString>
#include <QStringList>
#include <QVector>
#include <QPointF>

// 三个技能页
enum SkillPageId {
    SkillTransmission = 0, // 传播途径
    SkillSymptom      = 1, // 发病症状
    SkillAbility      = 2, // 特殊能力
    SkillPageCount    = 3
};

// 单个技能（六边形节点）的定义。
// 数值、文案、消耗均为占位，集中在 skilltree.cpp 的表格里，方便以后随意调整。
struct SkillDef
{
    QString id;            // 唯一标识
    QString name;          // 名称
    QString glyph;         // 六边形内显示的图标字符
    QString desc;          // 介绍文字
    int cost = 10;         // 消耗 DNA 点数

    // 解锁后对病毒属性的加成
    double dInfectivity = 0.0; // 传染性
    double dSeverity    = 0.0; // 严重性
    double dLethality   = 0.0; // 致命性
    double dCureSlow    = 0.0; // >0：减缓解药研发；足够大可使其倒退（仅特殊能力）

    int cell = 0;            // 蜂窝格编号 1..55（一列一列从上到下排序）
    QStringList neighbors;   // 显形传播用：相邻/前置-后继 id（解锁本技能后这些会显形）
    QStringList prereqs;     // 前置技能 id：必须全部解锁后本技能才可进化（传播途径/特殊能力用；发病症状为空=仅相邻）
    bool root = false;       // 初始即可见
};

// 构建三页技能树。数值带少量随机抖动以示“暂时随机生成”，结构（位置/相邻/名称）固定。
QVector<QVector<SkillDef>> buildSkillTree();

#endif // SKILLTREE_H
