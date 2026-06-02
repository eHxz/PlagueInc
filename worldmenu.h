#ifndef WORLDMENU_H
#define WORLDMENU_H

#include <QWidget>
#include <QVector>
#include <QColor>
#include <QString>
#include <QPointF>

class GameCore;
class QPushButton;
class QLabel;
class QStackedWidget;
class QStackedLayout;

// 扇形（环形）图：未受感染 / 受感染 / 死亡 占比，中心显示主导占比
class PieChart : public QWidget
{
public:
    explicit PieChart(QWidget *parent = nullptr);
    void setData(double healthy, double infected, double dead); // 占比 0~1
    QSize sizeHint() const override { return QSize(320, 320); }

protected:
    void paintEvent(QPaintEvent *) override;

private:
    double m_healthy = 1.0, m_infected = 0.0, m_dead = 0.0;
};

// 通用折线图：横坐标为时间（天，终点=当前天），纵坐标自定义
class LineChart : public QWidget
{
public:
    struct Series { QString name; QColor color; QVector<QPointF> pts; };
    explicit LineChart(QWidget *parent = nullptr);
    void setAxis(const QString &yLabel, double yMax, const QString &ySuffix);
    void setSeries(const QVector<Series> &series, double xMax);

protected:
    void paintEvent(QPaintEvent *) override;

private:
    QString m_yLabel, m_ySuffix;
    double m_yMax = 1.0, m_xMax = 1.0;
    QVector<Series> m_series;
};

// 传染病疫情报告：按状态分列展示地区
class ReportWidget : public QWidget
{
public:
    explicit ReportWidget(QWidget *parent = nullptr);
    void setLists(const QStringList &healthy, const QStringList &infected, const QStringList &dead);

protected:
    void paintEvent(QPaintEvent *) override;

private:
    QStringList m_healthy, m_infected, m_dead;
};

// 点击“世界概况”后弹出的半透明蓝色菜单：世界概况 + 其他数据
class WorldMenu : public QWidget
{
    Q_OBJECT
public:
    explicit WorldMenu(GameCore *core, QWidget *parent = nullptr);

    void openMenu(); // 打开并刷新

signals:
    void closed();

protected:
    void paintEvent(QPaintEvent *) override;

private slots:
    void onTabClicked(int tab);   // 0 世界概况 1 其他数据
    void refreshAll();

private:
    void buildOverviewPage();
    void buildDataPage();
    void showChart(int which);    // 0 列表 1-4 各图表

    GameCore *m_core;
    int m_tab = 0;
    QVector<QPushButton *> m_tabButtons;
    QStackedLayout *m_content = nullptr;

    // 世界概况页
    PieChart *m_pie = nullptr;
    QLabel *m_statHealthy = nullptr;
    QLabel *m_statInfected = nullptr;
    QLabel *m_statDead = nullptr;
    QLabel *m_cureTitle = nullptr;
    QLabel *m_cureSub = nullptr;
    QLabel *m_cureDays = nullptr;
    QLabel *m_curePercent = nullptr;

    // 其他数据页
    QStackedWidget *m_dataStack = nullptr;
    ReportWidget *m_report = nullptr;
    LineChart *m_popChart = nullptr;
    LineChart *m_cureChart = nullptr;
    LineChart *m_evoChart = nullptr;
};

#endif // WORLDMENU_H
