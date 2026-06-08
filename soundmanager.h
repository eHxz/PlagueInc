#ifndef SOUNDMANAGER_H
#define SOUNDMANAGER_H

#include <QObject>
#include <QHash>
#include <QString>

class QMediaPlayer;
class QAudioOutput;

// 全局音频管理：循环播放基础 BGM（Plague Blossom）+ 按需播放短音效。
//   音效：按键点击 / 关闭 / 气泡产生 / 气泡点击。
//   作为 qApp 事件过滤器，自动为所有 QAbstractButton 的点击发出“按键/关闭”音效，
//   无需逐个按钮接线。音频文件运行时从 可执行文件目录/soundtrack 读取
//   （CMake 在构建后会把 soundtrack 复制到该处）。
class SoundManager : public QObject
{
    Q_OBJECT
public:
    enum Sfx {
        ButtonClick = 0, // Buttonclick.ogg —— 按键点击
        ButtonClose,     // Buttonclose.ogg —— 关闭
        BubbleSpawn,     // Dna_push_sfx.ogg —— 气泡产生
        BubblePop        // Dna_pop_sfx.ogg —— 气泡点击（收集）
    };

    static SoundManager &instance();

    void startBgm();              // 开始循环播放全局 BGM（重复调用安全）
    void playSfx(Sfx s);          // 播放一次短音效

    void setMuted(bool m);        // 静音：暂停 BGM 并屏蔽音效
    bool isMuted() const { return m_muted; }

    void installButtonSounds();   // 安装 qApp 事件过滤器：按钮点击/关闭音效

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    explicit SoundManager(QObject *parent = nullptr);
    QString soundDir() const;                       // 可执行文件目录/soundtrack
    QMediaPlayer *sfxPlayer(Sfx s);                 // 懒加载对应音效播放器

    QMediaPlayer *m_bgm = nullptr;
    QAudioOutput *m_bgmOut = nullptr;
    QHash<int, QMediaPlayer *> m_sfx;
    QHash<int, QAudioOutput *> m_sfxOut;
    bool m_muted = false;
    bool m_filterInstalled = false;
};

#endif // SOUNDMANAGER_H
