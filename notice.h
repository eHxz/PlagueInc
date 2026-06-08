#ifndef NOTICE_H
#define NOTICE_H

#include <QWidget>
#include <QString>
#include <QPixmap>
#include <QRectF>
#include <QQueue>
#include <QHash>
#include <QSet>
#include "GameTypes.h"

class GameCore;

// ============================================================================
//  NoticePopup：主页面正中央的报告弹窗（notice.png 横幅）。
//  显示标题 + 正文，右下角为 OK 键；半透明背景遮挡并拦截点击（模态）。
//  弹窗显示期间游戏暂停，点击 OK 关闭后由外部恢复流速。
//  支持队列：多条报告依次显示，全部关闭后才发出 allDismissed。
// ============================================================================
class NoticePopup : public QWidget
{
    Q_OBJECT
public:
    explicit NoticePopup(QWidget *parent = nullptr);

    void enqueue(const QString &title, const QString &body); // 入队并（必要时）显示
    void dismissAll();                                       // 清空队列并隐藏（重开时用）
    bool isShowing() const { return !m_queue.isEmpty(); }

signals:
    void firstShown();   // 队列从空 -> 有内容（应暂停游戏）
    void allDismissed(); // 最后一条被关闭（应恢复流速）

protected:
    void paintEvent(QPaintEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;

private:
    QRectF bannerRect() const;       // 横幅在控件内的绘制矩形（保持比例、居中）
    QRectF okHotRect() const;        // OK 键的命中矩形（控件坐标）
    void showNext();                 // 显示队列中的下一条

    QPixmap m_pix;                   // notice.png 整图
    QRectF  m_src;                   // 图中非透明内容（横幅）的源矩形（像素）

    struct Item { QString title; QString body; };
    QQueue<Item> m_queue;
};

// ============================================================================
//  NoticeManager：监听 GameCore 信号，在满足触发条件时弹出对应报告。
//  报告文本从 notice.txt 加载（按编号取标题/正文），触发条件在代码中实现；
//  每条只触发一次（教程链 1->2、6->7、10->11 通过队列顺序显示实现）。
// ============================================================================
class NoticeManager : public QObject
{
    Q_OBJECT
public:
    NoticeManager(GameCore *core, NoticePopup *popup, QObject *parent = nullptr);

    void loadFromFile(const QString &path); // 解析 notice.txt（资源路径 :/notice.txt）
    void reset();                           // 重开游戏时清空已触发记录
    void onGameStarted();                   // 进入主游戏：报告 1（欢迎）+ 2（选择起始地区）

private:
    void fire(int id, const QString &regionName = QString(),
              const QString &milestone = QString());        // 触发编号为 id 的报告
    QString format(QString s, const QString &regionName,
                   const QString &milestone) const;          // 占位符替换
    QString mostInfectedRegionName() const;
    QString firstDeadRegionName() const;

    // 各信号处理
    void onGlobalStats(long long infected, long long dead, long long alive, long long total);
    void onCureProgress(double percent);
    void onNewRegionInfected(const QString &regionName);
    void onInfectionSurge();
    void onDiscovered(bool discovered);
    void onSkillsChanged();

    GameCore   *m_core;
    NoticePopup *m_popup;

    struct Notice { QString title; QString body; };
    QHash<int, Notice> m_notices; // 编号 -> 文本
    QSet<int> m_fired;            // 已触发的编号
    QSet<int> m_cureMilestones;   // 报告 25 已播报过的里程碑（25/50/75/100）

    int m_surgeCount = 0;          // 橙色气泡累计
    int m_infectedRegionCount = 0; // 被感染地区累计
};

#endif // NOTICE_H
