#ifndef GAMETYPES_H
#define GAMETYPES_H

#include <QString>
#include <QVector>

// 单个地区的运行时数据
struct RegionData
{
    QString name;                  // 地区名称
    long long totalPopulation = 0; // 总人口
    long long infectedCount = 0;   // 感染人数
    long long deadCount = 0;       // 死亡人数
    bool isInfected = false;       // 病毒是否已传入该地区
    QVector<int> neighbors;        // 相邻地区的下标（用于扩散）

    long long alive() const { return totalPopulation - deadCount; }
    long long healthy() const { return totalPopulation - infectedCount - deadCount; }
};

// 疾病全局属性（有效值 = 基础值 + 已解锁技能加成，由 GameCore 重新计算）
struct DiseaseStats
{
    double infectivity = 0.0; // 传染性：影响地区内增长与跨区传播
    double severity = 0.0;    // 严重性：影响“被发现”与解药研发速度
    double lethality = 0.0;   // 致命性：每天感染者转为死亡的比例
    // 海/空途径乘区（formula 形式二的载毒概率系数）：随【空气/水源传播】技能提升。
    // 由 GameCore::recomputeDisease() 计算后随 diseaseStatsChanged 一并下发给地图。
    // 默认值即 GameParams::kRouteModifierBase（极低）。
    double routeModifier = 0.10;
};

// 每天记录一份历史快照，供“其他数据”里的各曲线图绘制
struct HistoryPoint
{
    int day = 0;
    double healthyFrac = 1.0;  // 未受感染占比 0~1
    double infectedFrac = 0.0; // 受感染占比 0~1
    double deadFrac = 0.0;     // 死亡占比 0~1
    double cure = 0.0;         // 解药进度 0~100
    double infectivity = 0.0;  // 传染性
    double severity = 0.0;     // 严重性
    double lethality = 0.0;    // 致命性
};

#endif // GAMETYPES_H
