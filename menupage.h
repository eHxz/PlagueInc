#ifndef MENUPAGE_H
#define MENUPAGE_H

#include <QWidget>
#include <QLabel>

class VideoBackground;

// 开始菜单页：循环播放病原体视频 + 菜单图片（右下角）。
// 菜单图片左侧有四种难度选项（简单/普通/困难/天灾），
// 第一次点击选中并显示 ✓，再次点击同一难度确认进入游戏。
// 发射 difficultySelected 信号（0=简单 1=普通 2=困难 3=天灾），
// 由 MainWindow 接收后切换到命名页。
class MenuPage : public QWidget
{
    Q_OBJECT
public:
    explicit MenuPage(QWidget *parent = nullptr);

signals:
    void difficultySelected(int difficulty); // 0=简单 1=普通 2=困难 3=天灾

protected:
    void resizeEvent(QResizeEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void layoutContent();

    VideoBackground *m_videoBg = nullptr;
    QLabel *m_menuImage = nullptr;
    QLabel *m_checkLabel = nullptr;       // ✓ 选中标记
    QLabel *m_diffImage = nullptr;        // 难度预览图（显示在透明区域）
    int m_selectedDifficulty = -1;         // -1=未选, 0=简单, 1=普通, 2=困难, 3=天灾
};

#endif // MENUPAGE_H
