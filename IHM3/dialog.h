#ifndef DIALOG_H
#define DIALOG_H

#include <QDialog>
#include <QUdpSocket>
#include <QTimer>
#include <QHostAddress>

QT_BEGIN_NAMESPACE
namespace Ui { class Dialog; }
QT_END_NAMESPACE

class Dialog : public QDialog {
    Q_OBJECT

public:
    explicit Dialog(QWidget *parent = nullptr);
    ~Dialog();

private slots:
    void on_btn_EnvoyerAngle_clicked();
    void on_btn_MesurerDist_clicked();
    void on_btn_ScanUnique_clicked();
    void on_btn_Clear_clicked();
    void on_btn_ExportCsv_clicked();
    void on_spinBox_Portee_valueChanged(int value);

    void onUdpReadyRead();
    void onReplyTimeout();

private:
    Ui::Dialog *ui;
    QUdpSocket *udpSocket;
    QTimer *replyTimer;

    int currentAngle = 0;

    // Fiabilité UDP : délai d'attente et nouvel essai
    QString pendingCommand;
    int retryCount = 0;
    static constexpr int MAX_RETRIES = 2;

    bool validateEndpoint(QHostAddress &ip, quint16 &port);
    void sendCommandWithRetry(const QString &cmd, int timeoutMs);
    void transmitPendingCommand();
    void setStatus(const QString &msg, bool isError = false);
    void appendTableRow(int angle, const QString &distanceText);
};

#endif // DIALOG_H