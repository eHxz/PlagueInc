#ifndef GAMEPARAMS_H
#define GAMEPARAMS_H

// =====================================================================
//  全局可调参数（集中管理，便于以后随时修改平衡）
//  —— 内容对照 formula.md 的四套公式 + 海空交通可视化参数。
//  全部用 inline constexpr，改这里、重新编译即可生效；不需要改动逻辑代码。
//  注：标“复杂效果/技能”的项是留给技能系统的钩子，默认值即“未点出技能”的状态。
// =====================================================================
namespace GameParams {

// ---------------------------------------------------------------------
// 形式一：陆地接壤传播（每日判定）
//   单日陆地突破概率 = 基础边境流量 × 源国感染比例 × 传染性 × 陆地途径乘区 × 边境状态修饰
// ---------------------------------------------------------------------
inline constexpr double kBaseTraffic            = 0.07;  // 基础边境流量（0.05~0.1）
inline constexpr double kTraitModifierDefault   = 1.0;   // 陆地途径乘区初始值
inline constexpr double kTraitModifierAnimal    = 1.8;   // 进化【鸟类/牲畜传播】后（1.5~2.0）
inline constexpr double kBorderOpen             = 1.0;   // 边境开放
inline constexpr double kBorderClosed           = 0.0;   // 边境封锁
inline constexpr double kBorderClosedAnimalLeak = 0.05;  // 封锁后仍可被动物突破（点出无视封锁DNA时）

// ---------------------------------------------------------------------
// 形式二：海空交通传播（发车/起飞时判定）—— 本次交通系统使用
//   载毒概率 = 出发国感染比例 × 传染性 × 海/空途径乘区
// ---------------------------------------------------------------------
inline constexpr double kRouteModifierBase        = 0.10; // 海/空途径乘区初始（极低：海关滤网/水净化）
inline constexpr double kRouteModifierAirWaterLv1 = 1.0;  // 点出【空气/水源传播】等级1
inline constexpr double kRouteModifierAirWaterLv2 = 1.5;  // 等级2
inline constexpr int    kExternalSeedInfected     = 12;   // 红机/红船抵达后，在目标地区投放的初始感染人数

// ---------------------------------------------------------------------
// 形式三：政府封锁与恐慌指数（每日判定）
//   本国恐慌度 = 本国感染比例 × 严重性
//   全球恐慌度 = 全球感染比例 × 严重性 + 致命性
//   总恐慌指数 = 本国恐慌度×0.7 + 全球恐慌度×0.3，再乘“财富抗性”
// ---------------------------------------------------------------------
inline constexpr double kPanicLocalWeight     = 0.7;  // 本国权重
inline constexpr double kPanicGlobalWeight    = 0.3;  // 全球权重
// 三道封锁阈值：现在每个地区各自随机生成（高斯分布），下列为分布“均值”。
// 单个地区的三个阈值在 GameCore::resetGame() 抽样后会升序排列，保证
// 机场(最低) ≤ 港口 ≤ 全境(最高) 的收紧顺序。
inline constexpr double kLockAirportThreshold = 0.50; // 关闭机场阈值均值
inline constexpr double kLockPortThreshold    = 0.65; // 关闭港口阈值均值
inline constexpr double kLockBorderThreshold  = 0.80; // 全境封锁阈值均值
inline constexpr double kLockThresholdSigma   = 0.07; // 高斯分布标准差（各地区差异度）
// 财富抗性：越富有的国家越怕死、数值越小越早触发封锁。默认 1.0（无差别）；
// 想让富国更早封锁，可在 GameCore::wealthResistance() 里按地区返回 <1 的值。
inline constexpr double kWealthResistanceDefault = 1.0;

// ---------------------------------------------------------------------
// 形式四：每日死亡人数结算
//   单日死亡率   = 基础致死倍率 × 致命性
//   保底死亡人数 = 基础保底常数 × 致命性（每国每天的“绝对保底”死亡，受当前感染数封顶）
//   今日新增死亡 = 本国当前感染人数 × 单日死亡率 + 保底死亡人数
// ---------------------------------------------------------------------
inline constexpr double kBaseKillRate   = 0.03;     // 基础致死倍率（0.02~0.05）
inline constexpr double kBaseFloorDeaths = 50000.0; // 基础保底常数（≈五万）：保证有致命性时每日总有人死亡

// ---------------------------------------------------------------------
// 形式五：解药每日进度（每日结算，进度记为 0~100）
//   全球研发优先级 = 全球总恐慌度 × 全球存活比例
//   单日进度新增量 = (基础研发速率 × 全球研发优先级) ÷ 疾病抗药复杂性
//   基因重组：点出时一次性把当前进度倒退 kReshuffleSetbackMin~Max
// ---------------------------------------------------------------------
inline constexpr double kCureBaseRate        = 0.2; // 基础研发速率（0~100 刻度；满优先级且抗药=1 时每天 +0.2，约 500 天出解药）
inline constexpr double kCureResistanceBase  = 1.0; // 疾病抗药复杂性初始值（分母，越大研发越慢）
inline constexpr double kReshuffleSetbackMin = 5.0; // 基因重组单次倒退下限（进度点）
inline constexpr double kReshuffleSetbackMax = 10.0;// 基因重组单次倒退上限（进度点）

// ---------------------------------------------------------------------
// 海空交通可视化 / 调度（本次新增）
// ---------------------------------------------------------------------
inline constexpr double kVehicleTexPx     = 40.0;  // 载具贴图分辨率（像素；放大后仍清晰）
inline constexpr double kPlaneSceneSize   = 32.0;  // 飞机图标场景尺寸（场景单位，随地图缩放；= 轮船的 2 倍）
inline constexpr double kShipSceneSize    = 16.0;  // 轮船图标场景尺寸
inline constexpr double kTrafficScale     = 0.18;  // 全局发车频率缩放（越大越繁忙）。0.55→0.275→0.18：在减半基础上再放缓约 1.5 倍（平均发车间隔再拉长 ~1.5x）
inline constexpr int    kMaxVehicles      = 140;   // 同屏载具上限（超出则本日不再发车，防卡顿）
inline constexpr double kPlaneTravelSec   = 6.0;   // 飞机最长航线的航行时长（略放缓；其余按航程比例缩短）
inline constexpr double kShipTravelSec    = 13.0;  // 轮船最长航线的航行时长（更慢，贴近真实远洋航速）
inline constexpr double kMinTravelSec     = 1.6;   // 任意航线最短航行时长
// 载具动画与游戏速度联动：1× 速度对应的每日毫秒数。实际倍速 = kBaseDayMs / 当前每日间隔；
// 暂停（含进入菜单）时每日间隔为 0，倍速记为 0 -> 载具冻结。
inline constexpr double kBaseDayMs        = 1000.0;

// 感染航线：浅红 -> 随重复加深（仅“受感染”的载具会留下/加深红色航线）
//   飞机航线=实线，轮船航线=虚线（在 darkenTrail 按航线 type 区分）。
inline constexpr int    kTrailMaxHits   = 10;   // 加深到最深所需的感染通过次数
inline constexpr int    kTrailBaseAlpha = 45;   // 首次（最浅）不透明度
inline constexpr int    kTrailMaxAlpha  = 230;  // 最深不透明度
inline constexpr double kTrailWidth     = 2.4;  // 线宽（场景单位，随缩放；已加粗）
inline constexpr double kTrailDashLen   = 4.0;  // 轮船虚线段长（单位=线宽）
inline constexpr double kTrailGapLen    = 3.2;  // 轮船虚线间隔（单位=线宽）

// 轮船海洋寻路网格（A* 一次性预计算固定航线，避免轨迹混乱）
inline constexpr double kNavCell      = 6.0;    // 网格单元尺寸（场景单位；越小越精细、窄海越通畅）
inline constexpr double kNavArcticY   = 150.0;  // 此 y 以北视为北冰洋 -> 禁航
inline constexpr double kNavAntarcticY= 1465.0; // 此 y 以南（地图底缘/南极）-> 禁航
inline constexpr bool   kNavWrapX     = true;   // 允许东西向绕图（太平洋航线：右出左进）
inline constexpr int    kNavSnapRadius= 40;     // 港口落在陆地时，向四周搜索可航行海格的最大半径（格）
// 离岸与平滑（解决贴岸突变 / 折线生硬）
inline constexpr int    kNavClearTarget      = 3;   // 期望离岸格数：航线尽量保持的离岸余量
inline constexpr double kCoastPenalty        = 1.3; // A* 离岸代价系数（越大越远离海岸，减少贴岸频繁突变）
inline constexpr int    kSmoothSamplesPerSeg = 6;   // 航线样条每段细分点数（越大越平滑圆润）
// 形态学“海洋膨胀”：把贴海的薄陆地打通若干格，自动连通直布罗陀/苏伊士/博斯普鲁斯/丹麦海峡等
// 窄口与封闭海（地中海等）及栅格化产生的封闭小水塘，避免港口被困在死水里走直线穿陆。
inline constexpr int    kNavErodeCells       = 0.1;   // 海洋向陆地扩张的格数（0=关闭；越大连通越强但越可能打穿细窄地峡）
// 港口吸附时只认“大洋”：连通水域格数 ≥ 此值才算开阔海，避免被困在栅格化产生的封闭小水塘/内陆湖里。
inline constexpr int    kNavMinOceanCells    = 120;

} // namespace GameParams

#endif // GAMEPARAMS_H
