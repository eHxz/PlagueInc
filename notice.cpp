#include "notice.h"
#include "GameCore.h"
#include "soundmanager.h"

#include <QPainter>
#include <QMouseEvent>
#include <QTextOption>
#include <QFont>
#include <QFile>
#include <QTextStream>
#include <QStringConverter>
#include <QRegularExpression>
#include <QDebug>

// ============================================================================
//  NoticePopup
// ============================================================================
NoticePopup::NoticePopup(QWidget *parent)
    : QWidget(parent)
{
    setAttribute(Qt::WA_NoSystemBackground); // paintEvent 全权绘制（含半透明遮罩）
    setMouseTracking(true);                  // 用于 OK 键的悬停光标
    m_pix = QPixmap(":/mainui/notice.png");
    // notice.png 中非透明“横幅”的内容区（像素）：四周大量透明留白，只画这块。
    m_src = QRectF(581, 488, 1684, 351);
    hide();
}

QRectF NoticePopup::bannerRect() const
{
    if (m_src.height() <= 0 || width() <= 0 || height() <= 0)
        return QRectF();
    const double aspect = m_src.width() / m_src.height();
    double w = qMin(width() * 0.72, 1000.0);
    double h = w / aspect;
    const double maxH = height() * 0.40;
    if (h > maxH) { h = maxH; w = h * aspect; }
    const double x = (width() - w) / 2.0;
    const double y = (height() - h) / 2.0;
    return QRectF(x, y, w, h);
}

QRectF NoticePopup::okHotRect() const
{
    const QRectF b = bannerRect();
    return QRectF(b.x() + b.width() * 0.86, b.y() + b.height() * 0.79,
                  b.width() * 0.14, b.height() * 0.21);
}

void NoticePopup::enqueue(const QString &title, const QString &body)
{
    const bool wasEmpty = m_queue.isEmpty();
    m_queue.enqueue({title, body});
    if (wasEmpty) {
        show();
        raise();
        update();
        emit firstShown();
    }
}

void NoticePopup::dismissAll()
{
    m_queue.clear();
    hide();
}

void NoticePopup::showNext()
{
    if (m_queue.isEmpty()) {
        hide();
        emit allDismissed();
    } else {
        update();
    }
}

void NoticePopup::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::SmoothPixmapTransform);
    p.setRenderHint(QPainter::TextAntialiasing);

    // 半透明遮罩：压暗背景并聚焦弹窗
    p.fillRect(rect(), QColor(0, 0, 0, 120));

    if (m_queue.isEmpty() || m_pix.isNull())
        return;

    const QRectF b = bannerRect();
    p.drawPixmap(b, m_pix, m_src);

    const Item &it = m_queue.head();

    // 文字整体右移 notice 高度的 2/3，避开横幅左侧的图标（右边界保持不变）。
    const double shift = b.height() * (2.0 / 3.0);

    // 标题
    QRectF titleR(b.x() + b.width() * 0.12 + shift, b.y() + b.height() * 0.10,
                  b.width() * 0.84 - shift, b.height() * 0.26);
    QFont f = font();
    f.setBold(true);
    f.setPixelSize(qMax(14, int(b.height() * 0.16)));
    p.setFont(f);
    p.setPen(QColor(245, 249, 255));
    p.drawText(titleR, Qt::AlignLeft | Qt::AlignVCenter, it.title);

    // 正文（自动换行，避开右下角 OK 键区域）
    QRectF bodyR(b.x() + b.width() * 0.12 + shift, b.y() + b.height() * 0.40,
                 b.width() * 0.70 - shift, b.height() * 0.38);
    QFont bf = font();
    bf.setBold(false);
    bf.setPixelSize(qMax(11, int(b.height() * 0.10)));
    p.setFont(bf);
    p.setPen(QColor(210, 224, 238));
    QTextOption opt(Qt::AlignLeft | Qt::AlignTop);
    opt.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
    p.drawText(bodyR, it.body, opt);
}

void NoticePopup::mouseReleaseEvent(QMouseEvent *e)
{
    if (e->button() == Qt::LeftButton && !m_queue.isEmpty()
        && okHotRect().contains(e->position())) {
        SoundManager::instance().playSfx(SoundManager::ButtonClick);
        m_queue.dequeue();
        showNext();
    }
    // 不传给基类：弹窗为模态，吞掉一切点击，避免穿透到下方 HUD。
}

void NoticePopup::mouseMoveEvent(QMouseEvent *e)
{
    const bool overOk = !m_queue.isEmpty() && okHotRect().contains(e->position());
    setCursor(overOk ? Qt::PointingHandCursor : Qt::ArrowCursor);
}

// ============================================================================
//  NoticeManager
// ============================================================================
NoticeManager::NoticeManager(GameCore *core, NoticePopup *popup, QObject *parent)
    : QObject(parent)
    , m_core(core)
    , m_popup(popup)
{
    connect(m_core, &GameCore::globalStatsUpdated, this, &NoticeManager::onGlobalStats);
    connect(m_core, &GameCore::cureProgressChanged, this, &NoticeManager::onCureProgress);
    connect(m_core, &GameCore::newRegionInfected, this,
            [this](int, RegionData d) { onNewRegionInfected(d.name); });
    connect(m_core, &GameCore::infectionSurge, this,
            [this](int, RegionData) { onInfectionSurge(); });
    connect(m_core, &GameCore::discoveredChanged, this, &NoticeManager::onDiscovered);
    connect(m_core, &GameCore::skillsChanged, this, &NoticeManager::onSkillsChanged);
}

void NoticeManager::loadFromFile(const QString &path)
{
    m_notices.clear();
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "notice.txt open failed:" << path;
        return;
    }
    QTextStream in(&f);
    in.setEncoding(QStringConverter::Utf8);
    const QString all = in.readAll();
    const QStringList lines = all.split('\n');

    static const QRegularExpression titleRe(QStringLiteral("^(\\d+)\\.(.*)$"));
    int curId = -1;
    QString curTitle;
    QStringList curBody;
    auto flush = [&]() {
        if (curId > 0) {
            Notice n;
            n.title = curTitle.trimmed();
            n.body = curBody.join('\n').trimmed();
            m_notices.insert(curId, n);
        }
    };
    for (const QString &raw : lines) {
        const QString line = raw.trimmed();
        if (line.isEmpty())
            continue;
        const QRegularExpressionMatch m = titleRe.match(line);
        if (m.hasMatch()) {
            flush();
            curId = m.captured(1).toInt();
            curTitle = m.captured(2).trimmed();
            curBody.clear();
        } else if (line.startsWith(QChar(0xFF08))) { // 全角左括号开头 = 触发条件行，忽略
            continue;
        } else {
            curBody << line;
        }
    }
    flush();
}

void NoticeManager::reset()
{
    m_fired.clear();
    m_cureMilestones.clear();
    m_surgeCount = 0;
    m_infectedRegionCount = 0;
}

void NoticeManager::onGameStarted()
{
    fire(1); // 欢迎莅临瘟疫公司
    fire(2); // 起始地区选择（队列内紧随报告 1 之后显示 = “1 结束后触发”）
}

QString NoticeManager::format(QString s, const QString &regionName,
                              const QString &milestone) const
{
    s.replace(QStringLiteral("xxx"), m_core->diseaseName());
    if (!regionName.isEmpty())
        s.replace(QStringLiteral("---"), regionName);
    if (!milestone.isEmpty()) {
        s.replace(QString::fromUtf8("（25% 50% 75% 100%）"),
                  QString::fromUtf8("（") + milestone + QString::fromUtf8("）"));
        s.replace(QStringLiteral("25% 50% 75% 100%"), milestone);
    }
    s.replace(QStringLiteral("pax-12"), m_core->diseaseName());
    return s;
}

void NoticeManager::fire(int id, const QString &regionName, const QString &milestone)
{
    if (m_fired.contains(id))
        return;
    auto it = m_notices.find(id);
    if (it == m_notices.end())
        return;
    m_fired.insert(id);
    m_popup->enqueue(format(it->title, regionName, milestone),
                     format(it->body, regionName, milestone));
}

QString NoticeManager::mostInfectedRegionName() const
{
    QString name;
    long long best = -1;
    for (const RegionData &r : m_core->regions()) {
        if (r.infectedCount > best) {
            best = r.infectedCount;
            name = r.name;
        }
    }
    return name;
}

QString NoticeManager::firstDeadRegionName() const
{
    for (const RegionData &r : m_core->regions())
        if (r.deadCount > 0)
            return r.name;
    return QString();
}

void NoticeManager::onGlobalStats(long long infected, long long dead,
                                  long long alive, long long total)
{
    // 感染人数里程碑（由低到高入队）
    if (infected >= 200)         fire(8, mostInfectedRegionName());
    if (infected >= 5000)        fire(12, mostInfectedRegionName());
    if (infected >= 10000000LL)  fire(14, mostInfectedRegionName());
    if (infected >= 200000000LL) fire(15, mostInfectedRegionName());
    if (infected >= 2000000000LL) fire(17, mostInfectedRegionName());

    // 死亡人数里程碑
    if (dead >= 1)            fire(18, firstDeadRegionName()); // 第一宗死亡
    if (dead >= 75000000LL)   fire(20);
    if (dead >= 120000000LL)  fire(21);
    if (dead >= 300000000LL)  fire(22);

    // 世界上已无健康人类（健康 = 总 - 感染 - 死亡）
    if (infected > 0 && (total - infected - dead) <= 0)
        fire(23);

    // 幸存者不足 100 万且解药 < 90%
    if (alive < 1000000 && m_core->cureProgress() < 90.0)
        fire(24);
}

void NoticeManager::onCureProgress(double percent)
{
    if (percent >= 10.0)
        fire(19); // 提示：抵抗解药

    // 报告 25：解药完成度 25/50/75/100%，每个里程碑各播报一次
    auto it = m_notices.find(25);
    if (it != m_notices.end()) {
        for (int m : {25, 50, 75, 100}) {
            if (percent >= m && !m_cureMilestones.contains(m)) {
                m_cureMilestones.insert(m);
                const QString ms = QString::number(m) + "%";
                m_popup->enqueue(format(it->title, QString(), ms),
                                 format(it->body, QString(), ms));
            }
        }
    }
}

void NoticeManager::onNewRegionInfected(const QString &regionName)
{
    ++m_infectedRegionCount;
    if (m_infectedRegionCount == 1) {
        fire(3, regionName);             // 感染第一个国家
    } else if (m_infectedRegionCount == 2) {
        fire(9, regionName);             // 第二个被感染国家（经航线扩散）
    } else if (m_infectedRegionCount == 3) {
        fire(10, regionName);            // 第三个被感染国家
        fire(11);                        // 紧随其后：红色感染气泡提示
    }
}

void NoticeManager::onInfectionSurge()
{
    ++m_surgeCount;
    if (m_surgeCount == 1) {
        fire(4);                         // 第一个橙色气泡：DNA 气泡提示
    } else if (m_surgeCount == 2) {
        fire(6);                         // 第二个橙色气泡：开始传播
        fire(7);                         // 紧随其后：国家资料提示
    }
}

void NoticeManager::onDiscovered(bool discovered)
{
    if (discovered)
        fire(16); // 开始研发解药
}

void NoticeManager::onSkillsChanged()
{
    if (m_core->unlockedSkillCount() > 0)
        fire(5); // 第一次用 DNA 进化
}
