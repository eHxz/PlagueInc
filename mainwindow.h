#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QLabel>
#include <QMainWindow>
#include <QPushButton>
#include <QStackedWidget>
#include "GameCore.h"
#include "MapWidget.h"

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    // --- C同学需要实现的 UI 更新槽函数 ---
    void updateDayUI(int day);
    void updateStatsUI(long totalInfected, long totalDead);
    void updateDNAPoints(int points);

    // --- 按钮点击事件 ---
    void onStartClicked();

private:
    // 核心组件指针
    GameCore *m_core;
    MapWidget *m_map;

    void setupConnections(); // A同学预留，由C同学补全连接逻辑
    void applyStyle();       // C同学写QSS美化界面
};

#endif // MAINWINDOW_H
