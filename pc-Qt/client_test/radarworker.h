#ifndef RADARWORKER_H
#define RADARWORKER_H

/*------------------------------------------------------------------------------
 * RadarWorker — exécute les commandes réseau (code C bloquant) dans un
 * QThread séparé, pour que l'interface ne se fige jamais pendant l'attente.
 *
 * Le dialogue émet une requête -> slot process() (exécuté dans le thread du
 * worker) -> appel du module C -> signal finished() reçu dans le thread de
 * l'interface. Les connexions entre threads sont automatiquement « queued ».
 *----------------------------------------------------------------------------*/
#include <QObject>
#include <QString>
#include <QList>
#include <QMetaType>
#include <atomic>

struct RadarRequest {
    enum Kind { Angle, Dist, Scan };
    Kind    kind = Angle;
    QString host;
    quint16 port = 0;
    int     timeoutMs = 1000;
    int     retries = 2;
    int     angle = 0;                  // ANGLE
    int     start = 0, end = 180, step = 10;   // SCAN
};

struct ScanPoint {
    int    angle = 0;
    double cm = 0.0;
    bool   noEcho = false;
};

struct RadarResult {
    RadarRequest request;               // requête d'origine
    int     status = 0;                 // UDP_OK (0) ou code d'erreur
    QString errorText;                  // message lisible si status != 0
    QString raw;                        // réponse brute de la carte
    double  cm = 0.0;                   // DIST
    bool    noEcho = false;             // DIST
    QList<ScanPoint> points;            // SCAN
    qint64  elapsedMs = 0;
};

Q_DECLARE_METATYPE(RadarRequest)
Q_DECLARE_METATYPE(RadarResult)

class RadarWorker : public QObject
{
    Q_OBJECT
public:
    explicit RadarWorker(QObject *parent = nullptr) : QObject(parent) {}

    // Appelées depuis le thread de l'interface (variable atomique).
    void requestCancel() { m_cancel.store(true); }
    void resetCancel()   { m_cancel.store(false); }

public slots:
    void process(const RadarRequest &req);

signals:
    void attempt(int attempt, int maxAttempts);
    void finished(const RadarResult &result);

private:
    // Rappels passés au module C (voir udp_hooks_t).
    static int  cancelledCb(void *ctx);
    static void attemptCb(void *ctx, int attempt, int maxAttempts);

    std::atomic_bool m_cancel{false};
};

#endif // RADARWORKER_H
