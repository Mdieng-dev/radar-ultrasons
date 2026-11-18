#ifndef RADARWIDGET_H
#define RADARWIDGET_H

#include <QWidget>
#include <QVector>

struct RadarEcho {
    int angle;       // 0 à 180°
    double distance; // en cm
    int alpha;       // Opacité (255 -> 0 pour l'effet de rémanence)
};

class RadarWidget : public QWidget {
    Q_OBJECT

public:
    explicit RadarWidget(QWidget *parent = nullptr);

    void setSweepAngle(int angle);
    void addMeasurement(int angle, double distanceCm);
    void setMaxRange(double maxCm);
    void clearRadar();

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    int currentAngle = 0;
    double maxRangeCm = 200.0;
    QVector<RadarEcho> echoes;
};

#endif // RADARWIDGET_H