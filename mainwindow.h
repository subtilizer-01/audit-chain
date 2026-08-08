#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QWidget>
#include <QLabel>
#include <QPushButton>
#include <QLineEdit>
#include <QTimer>
#include <QVector>
#include <QPlainTextEdit>
#include "logchain.h"

// Per-entry render state driven by the verification sweep.
struct NodeView {
    int seq;
    QString actor, action, timestamp, detail, prevHash, entryHash;
    int state = 0;          // 0 idle, 1 verified, 2 broken
    qreal pulse = 0.0;      // 0..1, drives the flare when a node resolves
};

// The chain itself - drawn, not laid out. Everything below (nodes, connectors,
// the scan line, the severed link) is one paintEvent so it can animate freely.
class ChainCanvas : public QWidget {
    Q_OBJECT
public:
    explicit ChainCanvas(QWidget *parent = nullptr);

    void setNodes(const QVector<NodeView> &nodes);
    void setNodeState(int index, int state);
    void setScanY(qreal y)      { m_scanY = y; update(); }
    void setScanActive(bool on) { m_scanActive = on; update(); }
    void clearStates();

    int nodeCount() const   { return m_nodes.size(); }
    int nodeY(int index) const;

protected:
    void paintEvent(QPaintEvent *) override;

private slots:
    void tick();

private:
    QVector<NodeView> m_nodes;
    QTimer *m_anim;
    qreal m_phase = 0.0;        // drives travelling pulses on live links
    qreal m_scanY = -1;
    bool  m_scanActive = false;

    void drawGrid(QPainter &p);
    void drawNode(QPainter &p, const NodeView &n, int y);
    void drawLink(QPainter &p, int yFrom, int yTo, int stateAbove, bool severed);
};

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);

private slots:
    void onLoad();
    void onVerify();
    void onAppend();
    void onAnchor();
    void onAttack();
    void sweepStep();

private:
    LogChain m_chain;

    ChainCanvas   *m_canvas;
    QLabel        *m_banner;
    QPlainTextEdit*m_console;
    QLineEdit     *m_actorIn, *m_actionIn, *m_detailIn;

    // Telemetry readouts
    QLabel *m_statEntries, *m_statHead, *m_statAnchor, *m_statIntegrity, *m_statLast;

    QTimer *m_sweep;
    int m_sweepIndex = 0;
    int m_breakIndex = -1;
    QString m_breakReason;

    void buildUi();
    void refresh();
    void log(const QString &line, const QString &colour = QString());
    void setBanner(const QString &text, int level);   // 0 idle, 1 ok, 2 alert
    int  findFirstBreak(QString &reasonOut) const;
    void setTelemetry(const QString &integrity, const QString &colour);
};

#endif // MAINWINDOW_H