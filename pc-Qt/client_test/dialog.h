#ifndef DIALOG_H
#define DIALOG_H

#include <QDialog>
#include <QThread>

#include "radar_Thread.h"

QT_BEGIN_NAMESPACE
namespace Ui {
class Dialog;
}
QT_END_NAMESPACE

class Dialog : public QDialog
{
    Q_OBJECT

public:
    explicit Dialog(QWidget *parent = nullptr);
    ~Dialog() override;

signals:
    // Connecté (en file d'attente) au slot Radar_Thread::process.
    void radarRequested(const RadarRequest &req);

private slots:
    void onAngleClicked();
    void onDistClicked();
    void onScanClicked();
    void onCancelClicked();
    void onAttempt(int attempt, int maxAttempts);
    void onFinished(const RadarResult &res);

private:
    bool buildRequest(RadarRequest::Kind kind, RadarRequest &req);
    void startRequest(const RadarRequest &req, const QString &command);
    void setBusy(bool busy);
    void showError(const QString &text);
    void log(const QString &text);
    void loadSettings();
    void saveSettings() const;

    Ui::Dialog  *ui;
    QThread      m_thread;          // thread réseau
    Radar_Thread *m_worker = nullptr;
    bool         m_busy = false;
};
#endif // DIALOG_H
