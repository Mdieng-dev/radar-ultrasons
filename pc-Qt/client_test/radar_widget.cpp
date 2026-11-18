#include "radar_widget.h"
#include <QPainter>
#include <QtMath>

RadarWidget::RadarWidget(QWidget *parent) : QWidget(parent) {
    setMinimumSize(460, 270);
}

void RadarWidget::setMaxRange(double maxCm) {
    if (maxCm > 0) {
        maxRangeCm = maxCm;
        update();
    }
}

void RadarWidget::clearRadar() {
    echoes.clear();
    update();
}

void RadarWidget::setSweepAngle(int angle) {
    currentAngle = qBound(0, angle, 180);

    // Estompe progressivement les anciens points détectés (rémanence phosphore)
    for (int i = echoes.size() - 1; i >= 0; --i) {
        echoes[i].alpha -= 6;
        if (echoes[i].alpha <= 0) {
            echoes.removeAt(i);
        }
    }
    update();
}

void RadarWidget::addMeasurement(int angle, double distanceCm) {
    if (distanceCm > 0 && distanceCm <= maxRangeCm) {
        echoes.append({qBound(0, angle, 180), distanceCm, 255});
    }
    update();
}

void RadarWidget::paintEvent(QPaintEvent *event) {
    Q_UNUSED(event);
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    // Fond noir radar
    p.fillRect(rect(), QColor(8, 14, 10));

    // Centre en bas au milieu et rayon du demi-cercle
    const int margin = 32;
    double radius = qMin((width() - 2.0 * margin) / 2.0, height() - 1.6 * margin);
    if (radius <= 10) return;

    QPointF center(width() / 2.0, height() - margin * 0.8);

    // 1. Arcs concentriques (25%, 50%, 75%, 100%)
    QPen gridPen(QColor(0, 180, 70, 110), 1.2);
    p.setPen(gridPen);
    p.setFont(QFont("Consolas", 8));

    for (int i = 1; i <= 4; ++i) {
        double r = radius * (i / 4.0);
        QRectF arcRect(center.x() - r, center.y() - r, 2.0 * r, 2.0 * r);
        p.drawArc(arcRect, 0, 180 * 16);

        int distLabel = qRound(maxRangeCm * (i / 4.0));
        p.drawText(QPointF(center.x() + r - 18, center.y() + 14),
                   QString::number(distLabel) + "cm");
    }

    // 2. Rayons angulaires tous les 30° (0° à 180°)
    for (int deg = 0; deg <= 180; deg += 30) {
        double rad = qDegreesToRadians(static_cast<double>(deg));
        QPointF endPt(center.x() + radius * qCos(rad),
                      center.y() - radius * qSin(rad));
        p.drawLine(center, endPt);

        QPointF textPt(center.x() + (radius + 16) * qCos(rad) - 12,
                       center.y() - (radius + 14) * qSin(rad) + 4);
        p.drawText(textPt, QString::number(deg) + "°");
    }

    // 3. Traînée lumineuse derrière le faisceau
    QConicalGradient sweepGrad(center, currentAngle);
    sweepGrad.setColorAt(0.0,  QColor(0, 255, 100, 110));
    sweepGrad.setColorAt(0.08, QColor(0, 255, 100, 0));
    sweepGrad.setColorAt(1.0,  QColor(0, 255, 100, 0));

    p.setBrush(sweepGrad);
    p.setPen(Qt::NoPen);
    QRectF fullRect(center.x() - radius, center.y() - radius, 2.0 * radius, 2.0 * radius);
    p.drawPie(fullRect, 0, 180 * 16);

    // 4. Ligne principale du faisceau
    double curRad = qDegreesToRadians(static_cast<double>(currentAngle));
    QPointF beamEnd(center.x() + radius * qCos(curRad),
                    center.y() - radius * qSin(curRad));
    p.setPen(QPen(QColor(80, 255, 130), 2.5));
    p.drawLine(center, beamEnd);

    // 5. Échos des obstacles détectés (points rouges avec halo)
    for (const RadarEcho &echo : echoes) {
        double echoRad = qDegreesToRadians(static_cast<double>(echo.angle));
        double echoR = (echo.distance / maxRangeCm) * radius;
        QPointF echoPt(center.x() + echoR * qCos(echoRad),
                       center.y() - echoR * qSin(echoRad));

        p.setPen(Qt::NoPen);
        p.setBrush(QColor(255, 60, 60, echo.alpha / 3));
        p.drawEllipse(echoPt, 8, 8);

        p.setBrush(QColor(255, 80, 80, echo.alpha));
        p.drawEllipse(echoPt, 3.5, 3.5);
    }
}