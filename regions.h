#ifndef REGIONS_H
#define REGIONS_H

#include <QString>
#include <QStringList>
#include <QVector>
#include <QPair>
#include <QHash>

// =============================================================
// 地区配置中心（依据 regions.txt 划分的 58 个国家/地区）
// 想修改“分成几个地区 / 国家归属 / 接壤关系”，只改这一个文件即可。
// =============================================================
namespace Regions {

// ---------- 1) 地区名字（顺序与 regions.txt 一致）----------
inline const QStringList &names()
{
    static const QStringList kNames = {
        "美国",          // 0
        "加拿大",        // 1
        "墨西哥",        // 2
        "中美洲",        // 3
        "加勒比海",      // 4
        "格陵兰",        // 5
        "巴西",          // 6
        "阿根廷",        // 7
        "秘鲁",          // 8
        "哥伦比亚",      // 9
        "玻利维亚",      // 10
        "英国",          // 11
        "冰岛",          // 12
        "挪威",          // 13
        "瑞典",          // 14
        "芬兰",          // 15
        "法国",          // 16
        "西班牙",        // 17
        "意大利",        // 18
        "德国",          // 19
        "波兰",          // 20
        "俄罗斯",        // 21
        "乌克兰",        // 22
        "土耳其",        // 23
        "巴尔干国家",    // 24
        "波罗的海国家",  // 25
        "中欧",          // 26
        "埃及",          // 27
        "利比亚",        // 28
        "阿尔及利亚",    // 29
        "摩洛哥",        // 30
        "苏丹",          // 31
        "安哥拉",        // 32
        "博茨瓦纳",      // 33
        "津巴布韦",      // 34
        "马达加斯加",    // 35
        "南非",          // 36
        "中非",          // 37
        "东非",          // 38
        "西非",          // 39
        "沙特阿拉伯",    // 40
        "伊朗",          // 41
        "伊拉克",        // 42
        "中东",          // 43
        "阿富汗",        // 44
        "哈萨克斯坦",    // 45
        "中亚",          // 46
        "中国",          // 47
        "印度",          // 48
        "巴基斯坦",      // 49
        "日本",          // 50
        "韩国",          // 51
        "印度尼西亚",    // 52
        "菲律宾",        // 53
        "东南亚",        // 54
        "澳大利亚",      // 55
        "新西兰",        // 56
        "新几内亚"       // 57
    };
    return kNames;
}

inline int count() { return names().size(); }

// ---------- 2) 各地区人口（来自 regions.txt 第二列）----------
inline const QVector<long long> &populations()
{
    static const QVector<long long> kPop = {
        328000000LL,  // 0 美国
        35000000LL,   // 1 加拿大
        120000000LL,  // 2 墨西哥
        45000000LL,   // 3 中美洲
        40000000LL,   // 4 加勒比海
        50000LL,      // 5 格陵兰
        200000000LL,  // 6 巴西
        60000000LL,   // 7 阿根廷
        45000000LL,   // 8 秘鲁
        80000000LL,   // 9 哥伦比亚
        17000000LL,   // 10 玻利维亚
        65000000LL,   // 11 英国
        330000LL,     // 12 冰岛
        5000000LL,    // 13 挪威
        10000000LL,   // 14 瑞典
        5000000LL,    // 15 芬兰
        65000000LL,   // 16 法国
        56000000LL,   // 17 西班牙
        60000000LL,   // 18 意大利
        80000000LL,   // 19 德国
        38000000LL,   // 20 波兰
        144000000LL,  // 21 俄罗斯
        45000000LL,   // 22 乌克兰
        80000000LL,   // 23 土耳其
        60000000LL,   // 24 巴尔干国家
        60000000LL,   // 25 波罗的海国家
        80000000LL,   // 26 中欧
        90000000LL,   // 27 埃及
        60000000LL,   // 28 利比亚
        40000000LL,   // 29 阿尔及利亚
        35000000LL,   // 30 摩洛哥
        40000000LL,   // 31 苏丹
        30000000LL,   // 32 安哥拉
        2000000LL,    // 33 博茨瓦纳
        15000000LL,   // 34 津巴布韦
        26000000LL,   // 35 马达加斯加
        58000000LL,   // 36 南非
        150000000LL,  // 37 中非
        380000000LL,  // 38 东非
        350000000LL,  // 39 西非
        33000000LL,   // 40 沙特阿拉伯
        80000000LL,   // 41 伊朗
        38000000LL,   // 42 伊拉克
        100000000LL,  // 43 中东
        35000000LL,   // 44 阿富汗
        18000000LL,   // 45 哈萨克斯坦
        50000000LL,   // 46 中亚
        1400000000LL, // 47 中国
        1350000000LL, // 48 印度
        200000000LL,  // 49 巴基斯坦
        126000000LL,  // 50 日本
        75000000LL,   // 51 韩国
        270000000LL,  // 52 印度尼西亚
        106000000LL,  // 53 菲律宾
        260000000LL,  // 54 东南亚
        25000000LL,   // 55 澳大利亚
        5000000LL,    // 56 新西兰
        12000000LL    // 57 新几内亚
    };
    return kPop;
}

inline long long populationFor(int region)
{
    const auto &p = populations();
    return (region >= 0 && region < p.size()) ? p[region] : 0;
}

// ---------- 3) 接壤关系（依据真实地理：陆地相邻 + 海峡/近海邻接）----------
inline const QVector<QPair<int, int>> &adjacencyPairs()
{
    static const QVector<QPair<int, int>> kAdj = {
        // 美洲
        {0, 1}, {0, 2}, {0, 4}, {0, 21}, // 美国-加/墨/加勒比/俄(白令海峡)
        {1, 5},                          // 加拿大-格陵兰
        {2, 3}, {2, 4},
        {3, 4}, {3, 9},                  // 中美洲-哥伦比亚(巴拿马)
        {4, 9},
        {5, 12},                         // 格陵兰-冰岛
        {6, 7}, {6, 8}, {6, 9}, {6, 10}, {6, 39}, // 巴西-邻国 + 西非(大西洋)
        {7, 8}, {7, 10},
        {8, 9}, {8, 10},
        // 欧洲
        {11, 12}, {11, 13}, {11, 16}, {11, 19}, // 英国-冰岛/挪威/法/德
        {12, 13},
        {13, 14}, {13, 15},
        {14, 15}, {14, 19}, {14, 20}, {14, 25},
        {15, 21}, {15, 25},
        {16, 17}, {16, 18}, {16, 19}, {16, 26},
        {17, 18}, {17, 29}, {17, 30},    // 西班牙-阿尔及利亚/摩洛哥(直布罗陀)
        {18, 24}, {18, 26}, {18, 28}, {18, 29}, // 意大利-巴尔干/中欧/利比亚/突尼斯
        {19, 20}, {19, 26},
        {20, 21}, {20, 22}, {20, 25}, {20, 26},
        {21, 22}, {21, 45}, {21, 46}, {21, 47}, {21, 50}, {21, 51},
        {22, 23}, {22, 24}, {22, 26},
        {23, 24}, {23, 41}, {23, 42}, {23, 43}, {23, 46},
        {24, 26},
        // 北非与中东
        {27, 28}, {27, 31}, {27, 40}, {27, 43},
        {28, 29}, {28, 31}, {28, 37}, {28, 39},
        {29, 30}, {29, 39},
        {30, 39},
        {31, 37}, {31, 38},
        // 撒哈拉以南非洲
        {32, 33}, {32, 34}, {32, 36}, {32, 37},
        {33, 34}, {33, 36},
        {34, 35}, {34, 36}, {34, 37}, {34, 38},
        {35, 38},
        {37, 38}, {37, 39},
        {38, 43},                        // 东非-中东(曼德海峡)
        // 中东与中亚
        {40, 41}, {40, 42}, {40, 43},
        {41, 42}, {41, 43}, {41, 44}, {41, 46}, {41, 49},
        {42, 43},
        {44, 46}, {44, 47}, {44, 49},
        {45, 46}, {45, 47},
        {46, 47},
        // 亚洲与大洋洲
        {47, 48}, {47, 50}, {47, 51}, {47, 53}, {47, 54},
        {48, 49}, {48, 54},
        {50, 51},
        {52, 53}, {52, 54}, {52, 55}, {52, 57},
        {53, 54},
        {55, 56}, {55, 57},
        {56, 57}
    };
    return kAdj;
}

// 由接壤 pairs 生成每个地区的邻居列表
inline QVector<QVector<int>> neighborLists()
{
    QVector<QVector<int>> nb(count());
    for (const auto &p : adjacencyPairs()) {
        if (p.first >= 0 && p.first < count() && p.second >= 0 && p.second < count()) {
            nb[p.first].append(p.second);
            nb[p.second].append(p.first);
        }
    }
    return nb;
}

// ---------- 4) 国家 -> 地区 映射（由 regions.txt 生成，含少量近海territory补充）----------
inline const QHash<QString, int> &overrides()
{
    static const QHash<QString, int> kOv = {
        {"Baker Island", 0}, {"Howland Island", 0}, {"Jarvis Island", 0}, {"Johnston Atoll", 0},
        {"Midway Islands", 0}, {"Puerto Rico", 0}, {"US Virgin Islands", 0}, {"United States", 0},
        {"Wake Island", 0}, {"Canada", 1}, {"Saint Pierre and Miquelon", 1}, {"Mexico", 2}, {"Belize", 3},
        {"Costa Rica", 3}, {"El Salvador", 3}, {"Guatemala", 3}, {"Honduras", 3}, {"Nicaragua", 3},
        {"Panama", 3}, {"Anguilla", 4}, {"Antigua and Barbuda", 4}, {"Aruba", 4}, {"Bahamas", 4},
        {"Barbados", 4}, {"Bermuda", 4}, {"Bonair, Saint Eustachius and Saba", 4},
        {"British Virgin Islands", 4}, {"Cayman Islands", 4}, {"Cuba", 4}, {"Curaçao", 4}, {"Dominica", 4},
        {"Dominican Republic", 4}, {"Grenada", 4}, {"Guadeloupe", 4}, {"Haiti", 4}, {"Jamaica", 4},
        {"Martinique", 4}, {"Montserrat", 4}, {"Saint Barthelemy", 4}, {"Saint Kitts and Nevis", 4},
        {"Saint Lucia", 4}, {"Saint Martin", 4}, {"Saint Vincent and the Grenadines", 4},
        {"Sint Maarten", 4}, {"Trinidad and Tobago", 4}, {"Turks and Caicos Islands", 4}, {"Greenland", 5},
        {"Brazil", 6}, {"Argentina", 7}, {"Chile", 7}, {"Falkland Islands", 7}, {"Uruguay", 7},
        {"Ecuador", 8}, {"Peru", 8}, {"Colombia", 9}, {"French Guiana", 9}, {"Guyana", 9}, {"Suriname", 9},
        {"Venezuela", 9}, {"Bolivia", 10}, {"Paraguay", 10}, {"Guernsey", 11}, {"Ireland", 11},
        {"Isle of Man", 11}, {"Jersey", 11}, {"United Kingdom", 11}, {"Faroe Islands", 12}, {"Iceland", 12},
        {"Bouvet Island", 13}, {"Norway", 13}, {"Svalbard and Jan Mayen", 13}, {"Sweden", 14},
        {"Aland Islands", 15}, {"Finland", 15}, {"Andorra", 16}, {"France", 16}, {"Monaco", 16},
        {"Gibraltar", 17}, {"Portugal", 17}, {"Spain", 17}, {"Italy", 18}, {"Malta", 18}, {"San Marino", 18},
        {"Vatican City", 18}, {"Belgium", 19}, {"Denmark", 19}, {"Germany", 19}, {"Luxembourg", 19},
        {"Netherlands", 19}, {"Poland", 20}, {"Russia", 21}, {"Belarus", 22}, {"Moldova", 22},
        {"Ukraine", 22}, {"Türkiye", 23}, {"Albania", 24}, {"Bosnia and Herzegovina", 24}, {"Bulgaria", 24},
        {"Croatia", 24}, {"Cyprus", 24}, {"Greece", 24}, {"Kosovo", 24}, {"Montenegro", 24},
        {"North Macedonia", 24}, {"Romania", 24}, {"Serbia", 24}, {"Slovenia", 24}, {"Estonia", 25},
        {"Latvia", 25}, {"Lithuania", 25}, {"Austria", 26}, {"Czechia", 26}, {"Hungary", 26},
        {"Liechtenstein", 26}, {"Slovakia", 26}, {"Switzerland", 26}, {"Egypt", 27}, {"Libya", 28},
        {"Algeria", 29}, {"Tunisia", 29}, {"Morocco", 30}, {"Western Sahara", 30}, {"South Sudan", 31},
        {"Sudan", 31}, {"Angola", 32}, {"Botswana", 33}, {"Malawi", 34}, {"Mozambique", 34}, {"Zambia", 34},
        {"Zimbabwe", 34}, {"Comoros", 35}, {"Glorioso Islands", 35}, {"Juan De Nova Island", 35},
        {"Madagascar", 35}, {"Mauritius", 35}, {"Mayotte", 35}, {"Reunion", 35}, {"Seychelles", 35},
        {"Eswatini", 36}, {"Lesotho", 36}, {"Namibia", 36}, {"South Africa", 36}, {"Cameroon", 37},
        {"Central African Republic", 37}, {"Chad", 37}, {"Democratic Republic of Congo", 37},
        {"Equatorial Guinea", 37}, {"Gabon", 37}, {"Republic of Congo", 37}, {"Burundi", 38},
        {"Djibouti", 38}, {"Eritrea", 38}, {"Ethiopia", 38}, {"Kenya", 38}, {"Rwanda", 38}, {"Somalia", 38},
        {"Tanzania", 38}, {"Uganda", 38}, {"Benin", 39}, {"Burkina Faso", 39}, {"Cape Verde", 39},
        {"Côte d'Ivoire", 39}, {"Gambia", 39}, {"Ghana", 39}, {"Guinea", 39}, {"Guinea-Bissau", 39},
        {"Liberia", 39}, {"Mali", 39}, {"Mauritania", 39}, {"Niger", 39}, {"Nigeria", 39},
        {"Sao Tome and Principe", 39}, {"Senegal", 39}, {"Sierra Leone", 39}, {"Togo", 39},
        {"Saudi Arabia", 40}, {"Iran", 41}, {"Iraq", 42}, {"Bahrain", 43}, {"Israel", 43}, {"Jordan", 43},
        {"Kuwait", 43}, {"Lebanon", 43}, {"Oman", 43}, {"Palestinian Territories", 43}, {"Qatar", 43},
        {"Syria", 43}, {"United Arab Emirates", 43}, {"Yemen", 43}, {"Afghanistan", 44}, {"Kazakhstan", 45},
        {"Armenia", 46}, {"Azerbaijan", 46}, {"Georgia", 46}, {"Kyrgyzstan", 46}, {"Tajikistan", 46},
        {"Turkmenistan", 46}, {"Uzbekistan", 46}, {"China", 47}, {"Mongolia", 47}, {"Bangladesh", 48},
        {"Bhutan", 48}, {"British Indian Ocean Territory", 48}, {"India", 48}, {"Maldives", 48},
        {"Nepal", 48}, {"Sri Lanka", 48}, {"Pakistan", 49}, {"Japan", 50}, {"North Korea", 51},
        {"South Korea", 51}, {"Indonesia", 52}, {"Timor-Leste", 52}, {"Philippines", 53}, {"Brunei", 54},
        {"Cambodia", 54}, {"Lao People's Democratic Republic", 54}, {"Malaysia", 54}, {"Myanmar", 54},
        {"Singapore", 54}, {"Thailand", 54}, {"Vietnam", 54}, {"Australia", 55}, {"Christmas Island", 55},
        {"Cocos (Keeling) Islands", 55}, {"Heard Island and McDonald Islands", 55}, {"American Samoa", 56},
        {"Cook Islands", 56}, {"Federated States of Micronesia", 56}, {"French Polynesia", 56}, {"Guam", 56},
        {"Kiribati", 56}, {"Marshall Islands", 56}, {"Nauru", 56}, {"New Zealand", 56}, {"Niue", 56},
        {"Norfolk Island", 56}, {"Northern Mariana Islands", 56}, {"Palau", 56}, {"Pitcairn Islands", 56},
        {"Samoa", 56}, {"Tokelau", 56}, {"Tonga", 56}, {"Tuvalu", 56}, {"Wallis and Futuna", 56},
        {"Fiji", 57}, {"New Caledonia", 57}, {"Papua New Guinea", 57}, {"Solomon Islands", 57},
        {"Vanuatu", 57}
    };
    return kOv;
}

// 国家 -> 地区下标；未列入任何地区的国家返回 -1（中立陆地，不参与模拟）
inline int regionForCountry(const QString &country)
{
    const auto &ov = overrides();
    auto it = ov.find(country);
    return (it != ov.end()) ? it.value() : -1;
}

} // namespace Regions

#endif // REGIONS_H
