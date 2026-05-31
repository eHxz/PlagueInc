#ifndef GAMETYPES_H
#define GAMETYPES_H

#include <QString>

// 公用的国家数据结构
struct CountryData
{
    QString name;         // 国家名称
    long totalPopulation; // 总人口
    long infectedCount;   // 感染人数
    long deadCount;       // 死亡人数
    bool isBorderClosed;  // 边境是否关闭
};

#endif // GAMETYPES_H
