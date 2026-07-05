#include "menupage.h"
#include "diseasemenu.h" // VideoBackground
#include "soundmanager.h"

#include <QMouseEvent>
#include <QResizeEvent>
#include <QCoreApplication>

MenuPage::MenuPage(QWidget *parent)
    : QWidget(parent)
{
    // ---- 全屏循环视频背景 ----
    m_videoBg = new VideoBackground(this);
    m_videoBg->setSource(QCoreApplication::applicationDirPath() + "/media/pathogen.mp4");
    m_videoBg->start();
    m_videoBg->lower();

    // ---- 菜单图片（右下角）----
    m_menuImage = new QLabel(this);
    m_menuImage->setPixmap(QPixmap(":/menu.png"));
    m_menuImage->setScaledContents(true);
    m_menuImage->setAlignment(Qt::AlignCenter);
    m_menuImage->setCursor(Qt::PointingHandCursor);
    m_menuImage->setStyleSheet("background-color: rgba(0,0,0,30);");
    m_menuImage->installEventFilter(this);

    // ---- 难度预览图（显示在 menu.png 右侧透明区域）----
    m_diffImage = new QLabel(this);
    m_diffImage->setScaledContents(true);  // 拉伸填充，不裁剪
    m_diffImage->setAlignment(Qt::AlignCenter);
    m_diffImage->hide();
    m_diffImage->setStyleSheet("background: transparent;");

    // ---- ✓ 选中标记（默认隐藏）----
    m_checkLabel = new QLabel(this);
    m_checkLabel->setText("✓");
    m_checkLabel->setStyleSheet(
        "color: #00FF44; font-size: 22px; font-weight: bold;"
        "background: transparent;");
    m_checkLabel->setAlignment(Qt::AlignCenter);
    m_checkLabel->hide();

    // 默认选中普通难度
    m_selectedDifficulty = 1;
    m_checkLabel->show();
    m_diffImage->setPixmap(QPixmap(":/diff/2.png"));
    m_diffImage->show();

    layoutContent();
}

void MenuPage::resizeEvent(QResizeEvent *)
{
    layoutContent();
}

bool MenuPage::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_menuImage && event->type() == QEvent::MouseButtonPress) {
        QMouseEvent *me = static_cast<QMouseEvent *>(event);
        if (me->button() != Qt::LeftButton)
            return false;

        const int h = m_menuImage->height();
        if (h <= 0)
            return false;

        // 左侧四种难度区域（Y 占比）：
        //   简单: 27-42%   普通: 43-59%
        //   困难: 60-75%   天灾: 76-91%
        const double frac = (double)me->pos().y() / h;
        int difficulty = -1;

        if (frac >= 0.27 && frac < 0.43)
            difficulty = 0; // 简单
        else if (frac >= 0.43 && frac < 0.60)
            difficulty = 1; // 普通
        else if (frac >= 0.60 && frac < 0.76)
            difficulty = 2; // 困难
        else if (frac >= 0.76 && frac < 0.92)
            difficulty = 3; // 天灾

        if (difficulty < 0)
            return false;

        SoundManager::instance().playSfx(SoundManager::ButtonClick);

        if (m_selectedDifficulty == difficulty) {
            // 再次点击同一难度 → 确认，进入游戏
            emit difficultySelected(difficulty);
            return true;
        }

        // 首次选中或切换难度 → 显示 ✓ + 难度预览图
        m_selectedDifficulty = difficulty;
        m_checkLabel->show();

        // 加载对应难度预览图
        const QString paths[4] = {
            ":/diff/1.png", ":/diff/2.png", ":/diff/3.png", ":/diff/4.png"
        };
        m_diffImage->setPixmap(QPixmap(paths[difficulty]));
        m_diffImage->show();

        layoutContent();
        return true;
    }
    return QWidget::eventFilter(watched, event);
}

void MenuPage::layoutContent()
{
    const int W = width();
    const int H = height();

    // 视频铺满
    if (m_videoBg)
        m_videoBg->setGeometry(0, 0, W, H);

    if (m_menuImage) {
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
        const QPixmap pm = m_menuImage->pixmap();
#else
        const QPixmap *ppm = m_menuImage->pixmap();
        const QPixmap pm = ppm ? *ppm : QPixmap();
#endif
        if (!pm.isNull()) {
            const double imgW = pm.width();
            const double imgH = pm.height();
            const double scale = (W / 3.0) / imgW;
            const int w = qRound(imgW * scale);
            const int h = qRound(imgH * scale);
            const int x = W - w - qRound(W * 0.04);
            const int y = H - h - qRound(H * 0.04);
            m_menuImage->setGeometry(x, y, w, h);

            // 难度预览图：填充透明区域
            // 左上角 (14.7%, 47%)，右下角与 menu.png 一致 (100%, 100%)
            if (m_selectedDifficulty >= 0 && m_diffImage) {
                const int dx = x + qRound(w * 0.47);
                const int dy = y + qRound(h * 0.147);
                const int dw = w - qRound(w * 0.47);
                const int dh = h - qRound(h * 0.147);
                m_diffImage->setGeometry(dx, dy, dw, dh);
                m_diffImage->raise();
            }

            // ✓ 对号：放在图片左侧 1/3 处
            if (m_selectedDifficulty >= 0 && m_checkLabel) {
                const double centers[4] = {0.345, 0.51, 0.675, 0.835};
                const double cy = y + h * centers[m_selectedDifficulty];
                const int ckx = x + qRound(w * 0.33);
                const int ckh = qRound(h * 0.06);
                m_checkLabel->setGeometry(ckx, qRound(cy - ckh / 2.0),
                                          qRound(w * 0.08), ckh);
                m_checkLabel->raise();
            }
        }
    }
}
