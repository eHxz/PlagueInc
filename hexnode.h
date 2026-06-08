#ifndef HEXNODE_H
#define HEXNODE_H

#include <QWidget>
#include <QString>
#include <QPixmap>

// 技能树中的单个六边形节点。
class HexNode : public QWidget
{
    Q_OBJECT
public:
    explicit HexNode(const QString &id, QWidget *parent = nullptr);

    void setGlyph(const QString &g);
    void setPixmap(const QPixmap &pm); // 六边形技能图标（取自 graph/六边形，已旋转）
    void setState(bool unlocked, bool affordable);
    void setSelected(bool s);
    QString skillId() const { return m_id; }

signals:
    void clicked(QString id);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;

private:
    QString m_id;
    QString m_glyph;
    QPixmap m_pix;
    bool m_unlocked = false;
    bool m_affordable = false;
    bool m_selected = false;
};

#endif // HEXNODE_H
