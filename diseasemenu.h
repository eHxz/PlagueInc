#ifndef DISEASEMENU_H
#define DISEASEMENU_H

#include <QWidget>
#include <QVector>
#include <QSet>
#include <QPointF>
#include <QSizeF>
#include <QVideoFrame>
#include "GameTypes.h"

class GameCore;
class HexNode;
class QPushButton;
class QLabel;
class QFrame;
class QMediaPlayer;
class QVideoSink;

// 平顶六边形蜂窝骨架：用细线画出固定的 10 列 × (6/5 交替) = 55 格点阵，
// 并提供格中心坐标与“归一化提示点 -> 最近空格”的吸附。技能六边形落在这些格上。
class HexGrid : public QWidget
{
public:
    explicit HexGrid(QWidget *parent = nullptr);

    void setActive(bool a);        // 概况页不画蜂窝
    void setRightInset(int px);    // 右侧信息面板预留宽度（蜂窝不进入该区）

    int    columns() const { return kCols; }
    int    rowsInColumn(int col) const { return (col % 2 == 0) ? kRowsEven : kRowsOdd; }
    double side() const { return m_R; }                  // 六边形边长 R
    QSizeF cellSize() const;                             // 单格外接框 = (2R, √3R)
    QPointF cellCenter(int col, int row) const;
    QPointF cellCenterByIndex(int n) const;              // 蜂窝编号 1..55 -> 格中心
    static bool indexToColRow(int n, int &col, int &row); // 编号 1..55 -> (列,行)
    // 把 [0,1]² 提示点映射进蜂窝矩形，取最近的未占用格中心；占用集合以 col*100+row 记录
    QPointF snapNormalized(const QPointF &norm, QSet<int> &occupied,
                           int &outCol, int &outRow) const;

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    void recompute();              // 依当前尺寸与右侧预留算出 R 与蜂窝矩形原点

    static constexpr int kCols     = 10; // 宽：10 列（10 个六边形斜向下 30° 的水平跨度）
    static constexpr int kRowsEven = 6;  // 偶数列 6 格（高 = 6 个对边距离）
    static constexpr int kRowsOdd  = 5;  // 奇数列 5 格（上下各留半格缺口，不画）

    double  m_R = 10.0;            // 当前边长
    QPointF m_origin;             // 蜂窝矩形左上角（像素）
    int     m_rightInset = 0;
    bool    m_active = false;
};

// 底部状态条：DNA + 传染性 / 严重性 / 致命性
class StatBars : public QWidget
{
public:
    explicit StatBars(QWidget *parent = nullptr);
    void setDNA(int dna);
    void setStats(const DiseaseStats &s);
    void setPreview(const DiseaseStats &delta); // 选中技能时的属性增加量（浅色段）
    QSize sizeHint() const override { return QSize(800, 76); }

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    int m_dna = 0;
    DiseaseStats m_stats;
    DiseaseStats m_preview; // 增加量
};

// 循环播放的背景视频：用 QMediaPlayer + QVideoSink 取帧，自绘到控件上。
// 之所以不用 QVideoWidget：它在 Windows 上是原生子窗口，会盖住上层的概况面板。
// 这是普通的 Qt 绘制控件，能正确层叠在概况面板之下、菜单红色渐变之上。
class VideoBackground : public QWidget
{
    Q_OBJECT
public:
    explicit VideoBackground(QWidget *parent = nullptr);
    void setSource(const QString &filePath); // 本地文件路径
    void start();                            // 从头循环播放
    void stop();                             // 暂停（隐藏时省电）

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QMediaPlayer *m_player = nullptr;
    QVideoSink   *m_sink = nullptr;
    QVideoFrame   m_frame;
};

// 点击“疾病概况”后弹出的半透明菜单：4 个分页 + 六边形技能树。
class DiseaseMenu : public QWidget
{
    Q_OBJECT
public:
    explicit DiseaseMenu(GameCore *core, QWidget *parent = nullptr);

    void openMenu();         // 打开并切到“疾病概况”分页，刷新全部内容
    void openAtTab(int tab); // 打开并切到指定分页（0 概况 1 传播 2 症状 3 能力）

signals:
    void closed();

protected:
    void paintEvent(QPaintEvent *event) override;     // 半透明红色渐变
    void resizeEvent(QResizeEvent *event) override;   // 重新摆放六边形
    void hideEvent(QHideEvent *event) override;       // 关闭菜单时暂停背景视频

private slots:
    void onTabClicked(int tab);
    void onHexClicked(const QString &id);
    void onEvolveClicked();
    void rebuildSkills();
    void refreshStats();
    void refreshDNA();

private:
    void layoutHexes();
    void updateInfoPanel();
    int skillPageForTab(int tab) const; // 1/2/3 -> 0/1/2，其它返回 -1

    GameCore *m_core;
    int m_tab = 0; // 0 概况 1 传播 2 症状 3 能力

    QVector<QPushButton *> m_tabButtons;
    HexGrid *m_hexArea = nullptr;     // 透明蜂窝骨架，承载六边形
    QVector<HexNode *> m_hexes;       // 当前页的节点
    QLabel *m_overview = nullptr;     // 概况分页左上信息
    VideoBackground *m_videoBg = nullptr; // 概况(首页)背景循环视频

    QFrame *m_infoPanel = nullptr;
    QLabel *m_infoTitle = nullptr;
    QLabel *m_infoBody = nullptr;
    QPushButton *m_evolveBtn = nullptr;

    StatBars *m_statBars = nullptr;

    QString m_selectedSkill;
};

#endif // DISEASEMENU_H
