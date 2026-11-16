#include "radarworker.h"

#include <QElapsedTimer>
#include <QByteArray>

#include "radar_proto.h"   // module C (extern "C" dans l'en-tête)

int RadarWorker::cancelledCb(void *ctx)
{
    return static_cast<RadarWorker *>(ctx)->m_cancel.load() ? 1 : 0;
}

void RadarWorker::attemptCb(void *ctx, int attempt, int maxAttempts)
{
    // Émis depuis le thread du worker : Qt le livre à l'interface en différé.
    emit static_cast<RadarWorker *>(ctx)->attempt(attempt, maxAttempts);
}

void RadarWorker::process(const RadarRequest &req)
{
    // Ce code s'exécute dans le thread du worker : il peut bloquer sans
    // gêner l'interface.
    RadarResult res;
    res.request = req;

    const QByteArray host = req.host.toLatin1();   // chaîne C pour le module
    udp_hooks_t hooks{ &RadarWorker::cancelledCb, &RadarWorker::attemptCb, this };
    radar_conn_t conn{ host.constData(), req.port, req.timeoutMs, req.retries, &hooks };

    char raw[RADAR_REPLY_SIZE] = "";
    QElapsedTimer timer;
    timer.start();

    switch (req.kind) {
    case RadarRequest::Angle:
        res.status = radar_angle(&conn, req.angle, raw, sizeof raw);
        break;

    case RadarRequest::Dist: {
        double cm = 0.0;
        int noEcho = 0;
        res.status = radar_dist(&conn, &cm, &noEcho, raw, sizeof raw);
        res.cm = cm;
        res.noEcho = (noEcho != 0);
        break;
    }

    case RadarRequest::Scan: {
        radar_point_t pts[RADAR_MAX_POINTS];
        int n = 0;
        res.status = radar_scan(&conn, req.start, req.end, req.step,
                                pts, RADAR_MAX_POINTS, &n, raw, sizeof raw);
        for (int i = 0; i < n; ++i)
            res.points.append(ScanPoint{ pts[i].angle, pts[i].cm, pts[i].no_echo != 0 });
        break;
    }
    }

    res.elapsedMs = timer.elapsed();
    res.raw = QString::fromLatin1(raw);
    if (res.status != UDP_OK) {
        res.errorText = QString::fromUtf8(radar_strerror(res.status));
        const int wsa = udp_last_wsa_error();   // même thread : valeur fiable
        if (wsa != 0 && res.status != RADAR_ERR_BOARD && res.status != RADAR_ERR_PROTOCOL)
            res.errorText += QStringLiteral(" (code Winsock %1)").arg(wsa);
    }
    emit finished(res);
}
