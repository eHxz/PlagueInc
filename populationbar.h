#ifndef POPULATIONBAR_H
#define POPULATIONBAR_H

#include <QWidget>
#include <QString>

// 底部人口占比条：黑/红/蓝 = 死亡/感染/健康
class PopulationBar : public QWidget
{
    Q_OBJECT
public:
    explicit PopulationBar(QWidget *parent = nullptr);

    void setData(const QString &title, long long healthy, long long infected, long long dead);

    QSize sizeHint() const override { return QSize(400, 64); }

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QString m_title = "全球";
    long long m_healthy = 0;
    long long m_infected = 0;
    long long m_dead = 0;
};

#endif // POPULATIONBAR_H
