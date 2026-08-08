#include "mainwindow.h"

#include <QPainter>
#include <QPainterPath>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QFileDialog>
#include <QGraphicsDropShadowEffect>
#include <QDateTime>
#include <QScrollArea>
#include <QFrame>
#include <QtMath>

namespace {

const QColor BG      ("#05080a");
const QColor GRID    ("#0d151b");
const QColor PANEL   ("#0a1015");
const QColor EDGE    ("#16242e");
const QColor TXT     ("#c9dbe6");
const QColor DIM     ("#8aa5b5");
const QColor CYAN    ("#00e5ff");
const QColor GREEN   ("#00ff9c");
const QColor RED     ("#ff2d55");
const QColor AMBER   ("#ffb020");

const int NODE_H    = 120;
const int NODE_GAP  = 46;
const int LEFT_PAD  = 92;
const int TOP_PAD   = 40;

QString clip(const QString &h, int a = 10, int b = 8)
{
    if (h.size() <= a + b) return h;
    return h.left(a) + "\u2026" + h.right(b);
}

} // namespace

// ─────────────────────────────────────────────────────────────
// ChainCanvas
// ─────────────────────────────────────────────────────────────

ChainCanvas::ChainCanvas(QWidget *parent) : QWidget(parent)
{
    setMinimumWidth(560);
    m_anim = new QTimer(this);
    m_anim->setInterval(33);                       // ~30fps, enough for pulses
    connect(m_anim, &QTimer::timeout, this, &ChainCanvas::tick);
    m_anim->start();
}

void ChainCanvas::tick()
{
    m_phase += 0.022;
    if (m_phase > 1.0) m_phase -= 1.0;

    // Decay the flare on any node that just resolved.
    for (auto &n : m_nodes)
        if (n.pulse > 0.0) n.pulse = qMax(0.0, n.pulse - 0.045);

    update();
}

void ChainCanvas::setNodes(const QVector<NodeView> &nodes)
{
    m_nodes = nodes;
    setMinimumHeight(TOP_PAD * 2 + m_nodes.size() * NODE_H +
                     qMax(0, m_nodes.size() - 1) * NODE_GAP + 60);
    update();
}
void ChainCanvas::setNodeState(int index, int state)
{
    if (index < 0 || index >= m_nodes.size()) return;
    m_nodes[index].state = state;
    m_nodes[index].pulse = 1.0;
    update();
}

void ChainCanvas::clearStates()
{
    for (auto &n : m_nodes) { n.state = 0; n.pulse = 0.0; }
    update();
}

int ChainCanvas::nodeY(int index) const
{
    return TOP_PAD + index * (NODE_H + NODE_GAP);
}

void ChainCanvas::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.fillRect(rect(), BG);

    drawGrid(p);

    for (int i = 0; i < m_nodes.size(); i++) {
        if (i > 0) {
            const bool severed = (m_nodes[i].state == 2);
            drawLink(p, nodeY(i - 1) + NODE_H, nodeY(i), m_nodes[i - 1].state, severed);
        }
        drawNode(p, m_nodes[i], nodeY(i));
    }

    // Scan line - sweeps ahead of the verifier so the check reads as a process.
    if (m_scanActive && m_scanY >= 0) {
        QLinearGradient g(0, m_scanY - 26, 0, m_scanY + 26);
        g.setColorAt(0.0,  QColor(0, 229, 255, 0));
        g.setColorAt(0.5,  QColor(0, 229, 255, 60));
        g.setColorAt(1.0,  QColor(0, 229, 255, 0));
        p.fillRect(QRectF(0, m_scanY - 26, width(), 52), g);

        p.setPen(QPen(QColor(0, 229, 255, 190), 1));
        p.drawLine(QPointF(0, m_scanY), QPointF(width(), m_scanY));
    }
}

// Faint drifting grid so the background isn't dead space.
void ChainCanvas::drawGrid(QPainter &p)
{
    p.setPen(QPen(GRID, 1));
    const int step = 34;
    const int drift = int(m_phase * step);

    for (int x = -step + drift; x < width(); x += step)
        p.drawLine(x, 0, x, height());
    for (int y = -step + drift; y < height(); y += step)
        p.drawLine(0, y, width(), y);

    // Vignette toward the edges keeps focus on the chain.
    QLinearGradient v(0, 0, width(), 0);
    v.setColorAt(0.0, QColor(5, 8, 10, 220));
    v.setColorAt(0.2, QColor(5, 8, 10, 0));
    v.setColorAt(0.8, QColor(5, 8, 10, 0));
    v.setColorAt(1.0, QColor(5, 8, 10, 220));
    p.fillRect(rect(), v);
}

void ChainCanvas::drawNode(QPainter &p, const NodeView &n, int y)
{
    QColor accent = (n.state == 1) ? GREEN : (n.state == 2) ? RED : CYAN;
    if (n.state == 0) accent.setAlpha(120);

    const QRectF card(LEFT_PAD, y, width() - LEFT_PAD - 34, NODE_H);

    // Angled-corner panel - cut corners read as a HUD element, not a form box.
    const qreal cut = 20;
    QPainterPath body;
    body.moveTo(card.left() + cut, card.top());
    body.lineTo(card.right() - cut, card.top());
    body.lineTo(card.right(), card.top() + cut);
    body.lineTo(card.right(), card.bottom() - cut);
    body.lineTo(card.right() - cut, card.bottom());
    body.lineTo(card.left() + cut, card.bottom());
    body.lineTo(card.left(), card.bottom() - cut);
    body.lineTo(card.left(), card.top() + cut);
    body.closeSubpath();

    QLinearGradient bodyGrad(card.topLeft(), card.bottomRight());
    bodyGrad.setColorAt(0.0, QColor(13, 20, 26));
    bodyGrad.setColorAt(1.0, QColor(8, 13, 17));
    p.fillPath(body, bodyGrad);

    if (n.state != 0) {
        const int layers = 4;
        for (int i = layers; i >= 1; i--) {
            QColor g = accent;
            g.setAlpha(int((10 + 26 * n.pulse) * i / layers));
            p.setPen(QPen(g, i * 2.4));
            p.drawPath(body);
        }
    }

    p.setPen(QPen(accent, 1.4));
    p.drawPath(body);

    // Corner ticks - small marks at each cut corner
    p.setPen(QPen(accent, 2));
    p.drawLine(QPointF(card.left(), card.top() + cut),
               QPointF(card.left() + cut, card.top()));
    p.drawLine(QPointF(card.right() - cut, card.bottom()),
               QPointF(card.right(), card.bottom() - cut));

    // Hex node marker in the gutter
    const QPointF c(LEFT_PAD - 40, y + NODE_H / 2.0);
    QPolygonF hex;
    for (int i = 0; i < 6; i++) {
        const qreal a = M_PI / 3.0 * i - M_PI / 6.0;
        hex << QPointF(c.x() + 18 * qCos(a), c.y() + 18 * qSin(a));
    }
    p.setBrush(QColor(8, 14, 18));
    p.setPen(QPen(accent, 1.6));
    p.drawPolygon(hex);

    if (n.pulse > 0.0) {
        QColor flare = accent;
        flare.setAlpha(int(150 * n.pulse));
        p.setPen(QPen(flare, 2 + 7 * n.pulse));
        p.setBrush(Qt::NoBrush);
        p.drawPolygon(hex);
    }

    p.setFont(QFont("Consolas", 9, QFont::Bold));
    p.setPen(accent);
    p.drawText(QRectF(c.x() - 20, c.y() - 8, 40, 16), Qt::AlignCenter,
               QString("%1").arg(n.seq, 2, 10, QChar('0')));

    // Header row
    p.setFont(QFont("Consolas", 11, QFont::Bold));
    p.setPen(accent);
    p.drawText(QPointF(card.left() + 24, card.top() + 28),
               QString("ENTRY %1").arg(n.seq, 3, 10, QChar('0')));

    const QString tag = n.action.toUpper();
    p.setFont(QFont("Consolas", 10));
    const QRectF tagRect(card.left() + 140, card.top() + 13,
                         qMax(70, tag.size() * 10 + 20), 21);
    p.setPen(QPen(QColor(accent.red(), accent.green(), accent.blue(), 150), 1));
    p.setBrush(QColor(accent.red(), accent.green(), accent.blue(), 26));
    p.drawRoundedRect(tagRect, 10, 10);
    p.setPen(accent);
    p.drawText(tagRect, Qt::AlignCenter, tag);

    // Verdict marker
    if (n.state != 0) {
        p.setFont(QFont("Consolas", 10, QFont::Bold));
        p.setPen(accent);
        p.drawText(QRectF(card.right() - 130, card.top() + 12, 108, 20),
                   Qt::AlignRight | Qt::AlignVCenter,
                   n.state == 1 ? "\u2713 VERIFIED" : "\u2715 BROKEN");
    }

    // Meta line
    p.setFont(QFont("Consolas", 10));
    p.setPen(DIM);
    p.drawText(QPointF(card.left() + 24, card.top() + 54),
               QString("%1  \u2502  %2").arg(n.actor, n.timestamp));

    // Detail
    p.setPen(TXT);
    p.setFont(QFont("Consolas", 11));
    p.drawText(QRectF(card.left() + 24, card.top() + 62, card.width() - 48, 24),
               Qt::AlignLeft | Qt::AlignVCenter, n.detail);

    // Hash readout
    p.setFont(QFont("Consolas", 9));
    p.setPen(QColor(DIM.red(), DIM.green(), DIM.blue(), 220));
    p.drawText(QPointF(card.left() + 24, card.top() + 102),
               "PREV " + clip(n.prevHash));
    p.setPen(n.state == 2 ? RED : QColor(DIM.red(), DIM.green(), DIM.blue(), 220));
    p.drawText(QPointF(card.left() + 320, card.top() + 102),
               "SELF " + clip(n.entryHash));
}
// The connector. Intact links carry a travelling pulse; a severed link is drawn
// as a jagged break with the two ends pulled apart.
void ChainCanvas::drawLink(QPainter &p, int yFrom, int yTo, int stateAbove, bool severed)
{
    const qreal x = LEFT_PAD - 40;

    if (!severed) {
        QColor base = (stateAbove == 1) ? GREEN : CYAN;
        base.setAlpha(stateAbove == 1 ? 150 : 70);
        p.setPen(QPen(base, 1.6));
        p.drawLine(QPointF(x, yFrom), QPointF(x, yTo));

        // Travelling pulse, only on links that have been verified.
        if (stateAbove == 1) {
            const qreal t = m_phase;
            const qreal py = yFrom + (yTo - yFrom) * t;
            QRadialGradient g(QPointF(x, py), 13);
            g.setColorAt(0.0, QColor(0, 255, 156, 220));
            g.setColorAt(1.0, QColor(0, 255, 156, 0));
            p.setBrush(g);
            p.setPen(Qt::NoPen);
            p.drawEllipse(QPointF(x, py), 13, 13);
        }
        return;
    }

    // Severed: draw both stubs, then a torn gap between them.
    const qreal mid = (yFrom + yTo) / 2.0;
    p.setPen(QPen(QColor(255, 45, 85, 190), 1.6));
    p.drawLine(QPointF(x, yFrom), QPointF(x, mid - 13));
    p.drawLine(QPointF(x, mid + 13), QPointF(x, yTo));

    QPainterPath tear;
    tear.moveTo(x, mid - 13);
    tear.lineTo(x - 9,  mid - 6);
    tear.lineTo(x + 8,  mid - 1);
    tear.lineTo(x - 8,  mid + 5);
    tear.lineTo(x, mid + 13);
    p.setPen(QPen(RED, 2));
    p.drawPath(tear);

    QRadialGradient g(QPointF(x, mid), 26);
    g.setColorAt(0.0, QColor(255, 45, 85, 110));
    g.setColorAt(1.0, QColor(255, 45, 85, 0));
    p.setBrush(g);
    p.setPen(Qt::NoPen);
    p.drawEllipse(QPointF(x, mid), 26, 26);
}

// ─────────────────────────────────────────────────────────────
// MainWindow
// ─────────────────────────────────────────────────────────────

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent)
{
    buildUi();

    m_chain.append("ali",   "LOGIN",  "2026-08-07T09:00:00", "logged in from 192.168.1.5");
    m_chain.append("ali",   "DELETE", "2026-08-07T09:15:00", "deleted patient record 4471");
    m_chain.append("admin", "EXPORT", "2026-08-07T09:40:00", "exported audit report");
    m_chain.save("auditlog.txt");
    m_chain.saveAnchor("anchor.txt");

    refresh();
    log("session started", "#00e5ff");
    log("chain loaded \u2014 3 entries", "#3d5566");
    log("anchor written to anchor.txt", "#3d5566");
}

void MainWindow::buildUi()
{
    setWindowTitle("AUDIT CHAIN \u2014 integrity console");
    resize(1220, 760);
    setMinimumHeight(600);
    auto *central = new QWidget;
    central->setStyleSheet("background:#05080a;");
    auto *root = new QVBoxLayout(central);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // ── Header bar ─────────────────────────────────────────
    auto *header = new QWidget;
    header->setFixedHeight(64);
    header->setStyleSheet(
        "background:qlineargradient(x1:0,y1:0,x2:1,y2:0,"
        " stop:0 #070c10, stop:1 #05080a);"
        "border-bottom:1px solid #16242e;");
    auto *hl = new QHBoxLayout(header);
    hl->setContentsMargins(22, 0, 22, 0);

    auto *brand = new QLabel("A U D I T   C H A I N");
    brand->setStyleSheet("color:#00e5ff; font:700 16px 'Consolas'; letter-spacing:3px;");
    auto *bfx = new QGraphicsDropShadowEffect(brand);
    bfx->setBlurRadius(26); bfx->setOffset(0, 0);
    bfx->setColor(QColor(0, 229, 255, 190));
    brand->setGraphicsEffect(bfx);

    auto *tagline = new QLabel("  //  SHA-256 HASH-CHAINED LOG  \u00B7  TAMPER-EVIDENT");
    tagline->setStyleSheet("color:#7f9aab; font:11px 'Consolas'; letter-spacing:2px;");

    hl->addWidget(brand);
    hl->addWidget(tagline);
    hl->addStretch();
    root->addWidget(header);

    // ── Banner ─────────────────────────────────────────────
    m_banner = new QLabel;
    m_banner->setAlignment(Qt::AlignCenter);
    m_banner->setFixedHeight(46);
    auto *bannerWrap = new QWidget;
    auto *bwl = new QVBoxLayout(bannerWrap);
    bwl->setContentsMargins(22, 14, 22, 4);
    bwl->addWidget(m_banner);
    root->addWidget(bannerWrap);
    setBanner("SYSTEM READY  \u00B7  AWAITING VERIFICATION", 0);

    // ── Body: canvas | side rail ───────────────────────────
    auto *body = new QHBoxLayout;
    body->setContentsMargins(22, 6, 22, 0);
    body->setSpacing(16);

    auto *scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setStyleSheet(
        "QScrollArea{background:#05080a;}"
        "QScrollBar:vertical{background:#05080a;width:7px;}"
        "QScrollBar::handle:vertical{background:#16242e;border-radius:3px;min-height:40px;}"
        "QScrollBar::add-line,QScrollBar::sub-line{height:0;}");
    m_canvas = new ChainCanvas;
    scroll->setWidget(m_canvas);
    body->addWidget(scroll, 3);

    auto *rail = new QWidget;
    rail->setFixedWidth(310);
    rail->setStyleSheet("background:#080d11; border:1px solid #16242e; border-radius:5px;");
    auto *rl = new QVBoxLayout(rail);
    rl->setContentsMargins(16, 16, 16, 16);
    rl->setSpacing(10);

    auto sectionLabel = [](const QString &t) {
        auto *l = new QLabel(t);
        l->setStyleSheet("color:#7f9aab; font:700 10px 'Consolas'; letter-spacing:2px;");
        return l;
    };

    rl->addWidget(sectionLabel("TELEMETRY"));

    auto *statGrid = new QGridLayout;
    statGrid->setVerticalSpacing(9);
    statGrid->setHorizontalSpacing(8);

    auto addStat = [&](int row, const QString &key, QLabel *&valueOut,
                       const QString &initial, const QString &colour) {
        auto *k = new QLabel(key);
        k->setStyleSheet("color:#8aa5b5; font:10px 'Consolas'; letter-spacing:1px;");
        valueOut = new QLabel(initial);
        valueOut->setStyleSheet(QString("color:%1; font:11px 'Consolas';").arg(colour));
        valueOut->setAlignment(Qt::AlignRight);
        statGrid->addWidget(k, row, 0);
        statGrid->addWidget(valueOut, row, 1);
    };

    addStat(0, "ENTRIES",   m_statEntries,   "0",       "#c9dbe6");
    addStat(1, "HEAD",      m_statHead,      "\u2014",  "#00e5ff");
    addStat(2, "ANCHOR",    m_statAnchor,    "\u2014",  "#ffb020");
    addStat(3, "INTEGRITY", m_statIntegrity, "UNKNOWN", "#8aa5b5");
    addStat(4, "LAST SCAN", m_statLast,      "never",   "#8aa5b5");
    rl->addLayout(statGrid);

    auto *sep = new QFrame;
    sep->setFrameShape(QFrame::HLine);
    sep->setStyleSheet("background:#16242e; border:none; max-height:1px;");
    rl->addWidget(sep);

    rl->addWidget(sectionLabel("OPERATIONS"));

    auto mkBtn = [&](const QString &text, const QString &accent) {
        auto *b = new QPushButton(text);
        b->setCursor(Qt::PointingHandCursor);
        b->setFixedHeight(36);
        b->setStyleSheet(QString(
                             "QPushButton{background:#0a1015;color:%1;border:1px solid #16242e;"
                             " border-radius:4px;font:11px 'Consolas';letter-spacing:2px;text-align:left;"
                             " padding-left:12px;}"
                             "QPushButton:hover{border-color:%1;background:#0d151b;}"
                             "QPushButton:pressed{background:#060a0d;}").arg(accent));
        rl->addWidget(b);
        return b;
    };

    auto *bVerify = mkBtn("\u25B6  RUN VERIFICATION", "#00ff9c");
    auto *bLoad   = mkBtn("\u25A4  LOAD LOG FILE",    "#c9dbe6");
    auto *bAnchor = mkBtn("\u2693  WRITE ANCHOR",      "#ffb020");
    auto *bAttack = mkBtn("\u26A0  SIMULATE ATTACK",   "#ff2d55");

    connect(bVerify, &QPushButton::clicked, this, &MainWindow::onVerify);
    connect(bLoad,   &QPushButton::clicked, this, &MainWindow::onLoad);
    connect(bAnchor, &QPushButton::clicked, this, &MainWindow::onAnchor);
    connect(bAttack, &QPushButton::clicked, this, &MainWindow::onAttack);

    auto *sep2 = new QFrame;
    sep2->setFrameShape(QFrame::HLine);
    sep2->setStyleSheet("background:#16242e; border:none; max-height:1px;");
    rl->addWidget(sep2);

    rl->addWidget(sectionLabel("APPEND RECORD"));

    auto mkInput = [&](const QString &ph) {
        auto *e = new QLineEdit;
        e->setPlaceholderText(ph);
        e->setFixedHeight(32);
        e->setStyleSheet(
            "QLineEdit{background:#05080a;color:#c9dbe6;border:1px solid #16242e;"
            " border-radius:4px;padding:0 9px;font:11px 'Consolas';}"
            "QLineEdit:focus{border-color:#00e5ff;}");
        rl->addWidget(e);
        return e;
    };

    m_actorIn  = mkInput("actor");
    m_actionIn = mkInput("action");
    m_detailIn = mkInput("detail");

    auto *bAdd = mkBtn("+  APPEND TO CHAIN", "#00e5ff");
    connect(bAdd, &QPushButton::clicked, this, &MainWindow::onAppend);

    rl->addStretch();
    body->addWidget(rail);
    root->addLayout(body, 1);

    // ── Console ────────────────────────────────────────────
    auto *consoleWrap = new QWidget;
    auto *cwl = new QVBoxLayout(consoleWrap);
    cwl->setContentsMargins(22, 12, 22, 16);
    cwl->setSpacing(5);

    auto *cLabel = new QLabel("VERIFICATION LOG");
    cLabel->setStyleSheet("color:#7f9aab; font:700 10px 'Consolas'; letter-spacing:2px;");
    cwl->addWidget(cLabel);

    m_console = new QPlainTextEdit;
    m_console->setReadOnly(true);
    m_console->setFixedHeight(110);
    m_console->setStyleSheet(
        "QPlainTextEdit{background:#070c10;color:#a8c0ce;border:1px solid #16242e;"
        " border-radius:4px;font:11px 'Consolas';padding:8px;}"
        "QScrollBar:vertical{background:#070c10;width:7px;}"
        "QScrollBar::handle:vertical{background:#16242e;border-radius:3px;}"
        "QScrollBar::add-line,QScrollBar::sub-line{height:0;}");
    cwl->addWidget(m_console);
    root->addWidget(consoleWrap);

    setCentralWidget(central);

    m_sweep = new QTimer(this);
    m_sweep->setInterval(320);
    connect(m_sweep, &QTimer::timeout, this, &MainWindow::sweepStep);
}
void MainWindow::log(const QString &line, const QString &colour)
{
    const QString c = colour.isEmpty() ? "#a8c0ce" : colour;
    const QString ts = QDateTime::currentDateTime().toString("HH:mm:ss");
    m_console->appendHtml(QString(
                              "<span style='color:#5d7a8c; font-size:11px'>%1</span> "
                              "<span style='color:%2; font-size:11px'>&gt; %3</span>").arg(ts, c, line));
}
void MainWindow::setBanner(const QString &text, int level)
{
    QString fg = (level == 2) ? "#ff2d55" : (level == 1) ? "#00ff9c" : "#00e5ff";
    QString bg = (level == 2) ? "#12060a" : "#070d11";

    m_banner->setText(text);
    m_banner->setStyleSheet(QString(
                                "color:%1;background:%2;border:1px solid %1;border-radius:4px;"
                                "font:700 11px 'Consolas';letter-spacing:4px;").arg(fg, bg));

    auto *fx = new QGraphicsDropShadowEffect(m_banner);
    fx->setBlurRadius(level == 2 ? 44 : 26);
    fx->setOffset(0, 0);
    QColor c(fg);
    c.setAlpha(level == 2 ? 210 : 150);
    fx->setColor(c);
    m_banner->setGraphicsEffect(fx);
}

void MainWindow::setTelemetry(const QString &integrity, const QString &colour)
{
    m_statIntegrity->setText(integrity);
    m_statIntegrity->setStyleSheet(QString("color:%1; font:9px 'Consolas';").arg(colour));
}

void MainWindow::refresh()
{
    QVector<NodeView> nodes;
    for (int i = 0; i < m_chain.size(); i++) {
        const LogEntry &e = m_chain.at(i);
        NodeView n;
        n.seq       = e.getSeq();
        n.actor     = QString::fromStdString(e.getActor());
        n.action    = QString::fromStdString(e.getAction());
        n.timestamp = QString::fromStdString(e.getTimestamp());
        n.detail    = QString::fromStdString(e.getDetail());
        n.prevHash  = QString::fromStdString(e.getPrevHash());
        n.entryHash = QString::fromStdString(e.getEntryHash());
        nodes << n;
    }
    m_canvas->setNodes(nodes);

    m_statEntries->setText(QString::number(m_chain.size()));
    m_statHead->setText(m_chain.size()
                            ? clip(QString::fromStdString(m_chain.at(m_chain.size() - 1).getEntryHash()), 8, 6)
                            : "\u2014");
    m_statAnchor->setText("anchor.txt");
}

int MainWindow::findFirstBreak(QString &reasonOut) const
{
    for (int i = 0; i < m_chain.size(); i++) {
        const LogEntry &e = m_chain.at(i);

        if (e.computeHash() != e.getEntryHash()) {
            reasonOut = QString("ENTRY %1 \u2014 CONTENT MODIFIED").arg(e.getSeq());
            return i;
        }
        if (i == 0) {
            if (e.getPrevHash() != string(64, '0')) {
                reasonOut = "ENTRY 1 \u2014 GENESIS LINK BROKEN";
                return i;
            }
        } else if (e.getPrevHash() != m_chain.at(i - 1).getEntryHash()) {
            reasonOut = QString("ENTRY %1 \u2014 CHAIN LINK BROKEN").arg(e.getSeq());
            return i;
        }
    }
    return -1;
}

void MainWindow::onVerify()
{
    if (m_chain.size() == 0) { setBanner("NO CHAIN LOADED", 2); return; }

    m_canvas->clearStates();
    m_canvas->setScanActive(true);
    m_breakIndex = findFirstBreak(m_breakReason);
    m_sweepIndex = 0;

    setBanner("VERIFYING \u00B7 RECOMPUTING HASHES", 0);
    setTelemetry("SCANNING", "#00e5ff");
    log("verification started", "#00e5ff");
    m_sweep->start();
}

void MainWindow::sweepStep()
{
    if (m_sweepIndex >= m_canvas->nodeCount()) {
        m_sweep->stop();
        m_canvas->setScanActive(false);

        // Chain is internally consistent - now the only thing that can still
        // expose a rewrite is the external anchor.
        log("hash chain consistent \u2014 checking external anchor", "#ffb020");

        if (!m_chain.verifyAgainstAnchor("anchor.txt")) {
            setBanner("INTEGRITY VIOLATION \u00B7 HISTORY REWRITTEN", 2);
            setTelemetry("COMPROMISED", "#ff2d55");
            log("ANCHOR MISMATCH \u2014 the log was rebuilt to look valid", "#ff2d55");
            log("every hash recomputes correctly, but the anchored head does not match", "#ff2d55");
            for (int i = 0; i < m_canvas->nodeCount(); i++)
                m_canvas->setNodeState(i, 2);
        } else {
            setBanner(QString("CHAIN INTACT \u00B7 %1 ENTRIES VERIFIED").arg(m_chain.size()), 1);
            setTelemetry("VERIFIED", "#00ff9c");
            log("anchor matches \u2014 chain integrity confirmed", "#00ff9c");
        }
        m_statLast->setText(QDateTime::currentDateTime().toString("HH:mm:ss"));
        return;
    }

    m_canvas->setScanY(m_canvas->nodeY(m_sweepIndex) + NODE_H / 2.0);

    if (m_breakIndex >= 0 && m_sweepIndex == m_breakIndex) {
        m_canvas->setNodeState(m_sweepIndex, 2);
        m_sweep->stop();
        m_canvas->setScanActive(false);

        setBanner("INTEGRITY VIOLATION \u00B7 " + m_breakReason, 2);
        setTelemetry("COMPROMISED", "#ff2d55");
        log(QString("entry %1 \u2014 recomputed hash does not match stored hash")
                .arg(m_sweepIndex + 1), "#ff2d55");
        log("chain severed \u2014 everything below this point is untrusted", "#ff2d55");
        m_statLast->setText(QDateTime::currentDateTime().toString("HH:mm:ss"));
        return;
    }

    m_canvas->setNodeState(m_sweepIndex, 1);
    log(QString("entry %1 \u2014 hash verified").arg(m_sweepIndex + 1), "#00ff9c");
    m_sweepIndex++;
}

void MainWindow::onLoad()
{
    const QString path = QFileDialog::getOpenFileName(this, "Open log", ".", "Log files (*.txt)");
    if (path.isEmpty()) return;

    if (!m_chain.load(path.toStdString())) {
        setBanner("READ FAILED", 2);
        log("could not read " + path, "#ff2d55");
        return;
    }
    refresh();
    setBanner("LOG LOADED \u00B7 RUN VERIFICATION", 0);
    setTelemetry("UNKNOWN", "#3d5566");
    log(QString("loaded %1 entries from disk").arg(m_chain.size()), "#c9dbe6");
}

void MainWindow::onAppend()
{
    const QString actor  = m_actorIn->text().trimmed();
    const QString action = m_actionIn->text().trimmed();
    const QString detail = m_detailIn->text().trimmed();

    if (actor.isEmpty() || action.isEmpty()) {
        log("actor and action are required", "#ffb020");
        return;
    }

    const QString ts = QDateTime::currentDateTime().toString("yyyy-MM-ddTHH:mm:ss");
    m_chain.append(actor.toStdString(), action.toUpper().toStdString(),
                   ts.toStdString(), detail.toStdString());
    m_chain.save("auditlog.txt");

    m_actorIn->clear(); m_actionIn->clear(); m_detailIn->clear();
    refresh();
    setBanner("RECORD APPENDED", 0);
    log(QString("appended entry %1 \u2014 %2").arg(m_chain.size()).arg(action.toUpper()), "#00e5ff");
}

void MainWindow::onAnchor()
{
    m_chain.saveAnchor("anchor.txt");
    refresh();
    setBanner("ANCHOR WRITTEN", 0);
    setTelemetry("UNKNOWN", "#3d5566");
    log(QString("anchored entry %1 to anchor.txt").arg(m_chain.size()), "#ffb020");
    log("WARNING: anchor overwritten \u2014 whatever the chain says now is what "
        "gets trusted from here", "#ff2d55");
    log("in production this file must live where the logging process cannot "
        "write to it, or an attacker simply re-anchors their forgery", "#ffb020");
}

void MainWindow::onAttack()
{
    m_chain.simulateFullRewrite();
    m_chain.save("auditlog.txt");
    refresh();
    setBanner("ATTACK EXECUTED \u00B7 ALL HASHES REBUILT", 2);
    setTelemetry("UNKNOWN", "#3d5566");
    log("attacker recomputed every hash downstream", "#ff2d55");
    log("chain now passes internal verification \u2014 run VERIFY", "#ffb020");
}