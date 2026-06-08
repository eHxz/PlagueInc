#include "soundmanager.h"

#include <QCoreApplication>
#include <QMediaPlayer>
#include <QAudioOutput>
#include <QUrl>
#include <QFileInfo>
#include <QAbstractButton>
#include <QMouseEvent>
#include <QEvent>

namespace {
const char *kBgmFile   = "05 Plague Blossom.mp3";
const char *kSfxFiles[] = {
    "Buttonclick.ogg",  // ButtonClick
    "Buttonclose.ogg",  // ButtonClose
    "Dna_push_sfx.ogg", // BubbleSpawn（气泡产生）
    "Dna_pop_sfx.ogg",  // BubblePop（气泡点击）
};
const double kBgmVolume = 0.45;
const double kSfxVolume = 0.85;
}

SoundManager &SoundManager::instance()
{
    static SoundManager s;
    return s;
}

SoundManager::SoundManager(QObject *parent)
    : QObject(parent)
{
}

QString SoundManager::soundDir() const
{
    // 构建后 soundtrack 被复制到可执行文件同级目录。
    return QCoreApplication::applicationDirPath() + "/soundtrack/";
}

void SoundManager::startBgm()
{
    if (!m_bgm) {
        m_bgm = new QMediaPlayer(this);
        m_bgmOut = new QAudioOutput(this);
        m_bgmOut->setVolume(kBgmVolume);
        m_bgm->setAudioOutput(m_bgmOut);
        m_bgm->setLoops(QMediaPlayer::Infinite); // 循环播放
        const QString path = soundDir() + kBgmFile;
        if (QFileInfo::exists(path))
            m_bgm->setSource(QUrl::fromLocalFile(path));
    }
    if (!m_muted && m_bgm->source().isValid())
        m_bgm->play();
}

QMediaPlayer *SoundManager::sfxPlayer(Sfx s)
{
    auto it = m_sfx.find(s);
    if (it != m_sfx.end())
        return it.value();

    QMediaPlayer *p = new QMediaPlayer(this);
    QAudioOutput *o = new QAudioOutput(this);
    o->setVolume(kSfxVolume);
    p->setAudioOutput(o);
    const QString path = soundDir() + kSfxFiles[s];
    if (QFileInfo::exists(path))
        p->setSource(QUrl::fromLocalFile(path));
    m_sfx.insert(s, p);
    m_sfxOut.insert(s, o);
    return p;
}

void SoundManager::playSfx(Sfx s)
{
    if (m_muted)
        return;
    QMediaPlayer *p = sfxPlayer(s);
    if (!p->source().isValid())
        return;
    p->stop();           // 重置到开头，允许快速连点重播
    p->setPosition(0);
    p->play();
}

void SoundManager::setMuted(bool m)
{
    m_muted = m;
    if (m_bgm) {
        if (m)
            m_bgm->pause();
        else if (m_bgm->source().isValid())
            m_bgm->play();
    }
}

void SoundManager::installButtonSounds()
{
    if (m_filterInstalled)
        return;
    if (QCoreApplication::instance()) {
        QCoreApplication::instance()->installEventFilter(this);
        m_filterInstalled = true;
    }
}

bool SoundManager::eventFilter(QObject *watched, QEvent *event)
{
    if (event->type() == QEvent::MouseButtonRelease) {
        QMouseEvent *me = static_cast<QMouseEvent *>(event);
        if (me->button() == Qt::LeftButton) {
            if (QAbstractButton *btn = qobject_cast<QAbstractButton *>(watched)) {
                // 仅当“在按钮内松开”才算一次有效点击。
                if (btn->isEnabled() && btn->rect().contains(me->position().toPoint())) {
                    const QString on = btn->objectName();
                    const QString tx = btn->text();
                    const bool isClose = on.contains("Close", Qt::CaseInsensitive)
                                         || tx.contains(QChar(0x2716))  // ✖
                                         || tx.contains(QChar(0x00D7))  // ×
                                         || tx.contains("关闭");
                    playSfx(isClose ? ButtonClose : ButtonClick);
                }
            }
        }
    }
    return QObject::eventFilter(watched, event);
}
