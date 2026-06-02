#ifndef DISEASEMENU_H
#define DISEASEMENU_H

#include <QWidget>
#include <QVector>
#include "GameTypes.h"

class GameCore;
class HexNode;
class QPushButton;
class QLabel;
class QFrame;

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
    QWidget *m_hexArea = nullptr;     // 透明，承载六边形
    QVector<HexNode *> m_hexes;       // 当前页的节点
    QLabel *m_overview = nullptr;     // 概况分页左上信息

    QFrame *m_infoPanel = nullptr;
    QLabel *m_infoTitle = nullptr;
    QLabel *m_infoBody = nullptr;
    QPushButton *m_evolveBtn = nullptr;

    StatBars *m_statBars = nullptr;

    QString m_selectedSkill;
};

#endif // DISEASEMENU_H
