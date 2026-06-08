#include "mainwindow.h"
#include "soundmanager.h"

#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    // 全局音频：循环 BGM + 为所有按钮自动接入点击/关闭音效。
    SoundManager::instance().installButtonSounds();
    SoundManager::instance().startBgm();

    MainWindow w;
    w.show();
    return a.exec();
}
