#include "dialog.h"
#include "ui_dialog.h"
#include <QNetworkDatagram>
#include <QStringList>
#include <QHeaderView>
#include <QTableWidgetItem>
#include <QFileDialog>
#include <QFile>
#include <QTextStream>

Dialog::Dialog(QWidget *parent)
    : QDialog(parent), ui(new Ui::Dialog) {
    ui->setupUi(this);

    udpSocket = new QUdpSocket(this);
    replyTimer = new QTimer(this);
    replyTimer->setSingleShot(true);

    // Liaison sur le port local 5001 (IPv4), avec repli sur un port libre si occupé
    if (!udpSocket->bind(QHostAddress::AnyIPv4, 5001)) {
        if (!udpSocket->bind(QHostAddress::AnyIPv4, 0)) {
            setStatus("Erreur : impossible d'ouvrir le socket UDP local.", true);
        }
    }

    connect(udpSocket, &QUdpSocket::readyRead, this, &Dialog::onUdpReadyRead);
    connect(replyTimer, &QTimer::timeout, this, &Dialog::onReplyTimeout);

    ui->tableWidget_Mesures->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    ui->radarWidget->setMaxRange(ui->spinBox_Portee->value());
}

Dialog::~Dialog() {
    if (replyTimer->isActive()) {
        replyTimer->stop();
    }
    if (udpSocket->isOpen()) {
        udpSocket->close();
    }
    delete ui;
}

void Dialog::setStatus(const QString &msg, bool isError) {
    ui->label_Statut->setText(msg);
    if (isError) {
        ui->label_Statut->setStyleSheet("color: #c62828; font-weight: bold;");
    } else {
        ui->label_Statut->setStyleSheet("color: #1b5e20;");
    }
}

bool Dialog::validateEndpoint(QHostAddress &ip, quint16 &port) {
    QString ipText = ui->lineEdit_IP->text().trimmed();
    QString portText = ui->lineEdit_Port->text().trimmed();

    if (!ip.setAddress(ipText)) {
        setStatus("Erreur de saisie : adresse IP invalide (" + ipText + ").", true);
        return false;
    }

    bool okPort = false;
    uint p = portText.toUInt(&okPort);
    if (!okPort || p == 0 || p > 65535) {
        setStatus("Erreur de saisie : port UDP invalide (1 à 65535).", true);
        return false;
    }

    port = static_cast<quint16>(p);
    return true;
}

void Dialog::sendCommandWithRetry(const QString &cmd, int timeoutMs) {
    QHostAddress ip;
    quint16 port = 0;
    if (!validateEndpoint(ip, port)) {
        return;
    }

    pendingCommand = cmd;
    retryCount = 0;
    replyTimer->setInterval(timeoutMs);
    transmitPendingCommand();
}

void Dialog::transmitPendingCommand() {
    QHostAddress ip;
    quint16 port = 0;
    if (!validateEndpoint(ip, port)) {
        return;
    }

    qint64 sent = udpSocket->writeDatagram(pendingCommand.toUtf8(), ip, port);
    if (sent < 0) {
        setStatus("Erreur d'envoi UDP : " + udpSocket->errorString(), true);
        return;
    }

    replyTimer->start();
    QString cleanCmd = pendingCommand.trimmed();
    if (retryCount == 0) {
        setStatus("Envoi : " + cleanCmd + " (attente de réponse...)");
    } else {
        setStatus("Nouvel essai (" + QString::number(retryCount) + "/" +
                      QString::number(MAX_RETRIES) + ") : " + cleanCmd, true);
    }
}

void Dialog::onReplyTimeout() {
    if (retryCount < MAX_RETRIES) {
        retryCount++;
        transmitPendingCommand();
    } else {
        setStatus("Timeout : aucune réponse reçue pour '" + pendingCommand.trimmed() + "'.", true);
    }
}

void Dialog::on_btn_EnvoyerAngle_clicked() {
    int angle = ui->spinBox_AngleCible->value();
    currentAngle = angle;
    ui->label_Angle->setText(QString::number(angle) + " °");
    ui->radarWidget->setSweepAngle(angle);

    sendCommandWithRetry("ANGLE " + QString::number(angle) + "\n", 1000);
}

void Dialog::on_btn_MesurerDist_clicked() {
    sendCommandWithRetry("DIST\n", 1000);
}

void Dialog::on_btn_ScanUnique_clicked() {
    int debut = ui->spinBox_AngleDebut->value();
    int fin   = ui->spinBox_AngleFin->value();
    int pas   = ui->spinBox_PasScan->value();

    if (debut > fin) {
        setStatus("Erreur de saisie : l'angle de départ doit être inférieur ou égal à l'angle d'arrivée.", true);
        return;
    }
    if (pas <= 0) {
        setStatus("Erreur de saisie : le pas doit être strictement positif.", true);
        return;
    }

    int nbPoints = ((fin - debut) / pas) + 1;
    // Marge de temps adaptée au nombre de points (environ 180 ms par point sur STM32)
    int timeoutMs = qMax(2500, nbPoints * 250);

    ui->tableWidget_Mesures->setRowCount(0);
    QString cmd = QString("SCAN %1 %2 %3\n").arg(debut).arg(fin).arg(pas);
    sendCommandWithRetry(cmd, timeoutMs);
}

void Dialog::on_btn_Clear_clicked() {
    ui->radarWidget->clearRadar();
    ui->tableWidget_Mesures->setRowCount(0);
    ui->label_Distance->setText("-- cm");
    setStatus("Affichage radar et tableau réinitialisés.");
}

void Dialog::on_spinBox_Portee_valueChanged(int value) {
    ui->radarWidget->setMaxRange(value);
}

void Dialog::appendTableRow(int angle, const QString &distanceText) {
    int row = ui->tableWidget_Mesures->rowCount();
    ui->tableWidget_Mesures->insertRow(row);
    ui->tableWidget_Mesures->setItem(row, 0, new QTableWidgetItem(QString::number(angle)));
    ui->tableWidget_Mesures->setItem(row, 1, new QTableWidgetItem(distanceText));
    ui->tableWidget_Mesures->scrollToBottom();
}

void Dialog::onUdpReadyRead() {
    while (udpSocket->hasPendingDatagrams()) {
        QNetworkDatagram datagram = udpSocket->receiveDatagram();
        QString payload = QString::fromUtf8(datagram.data()).trimmed();

        replyTimer->stop();

        const QStringList lines = payload.split('\n', Qt::SkipEmptyParts);
        for (const QString &rawLine : lines) {
            QString line = rawLine.trimmed();

            if (line == "OK") {
                setStatus("Servomoteur orienté à " + QString::number(currentAngle) + "° (OK).");
                continue;
            }

            if (line == "DIST TIMEOUT") {
                ui->label_Distance->setText("Hors portée");
                appendTableRow(currentAngle, "TIMEOUT");
                setStatus("Aucun écho détecté à " + QString::number(currentAngle) + "° (DIST TIMEOUT).");
                continue;
            }

            if (line.startsWith("ERR")) {
                setStatus("Erreur carte STM32 : " + line, true);
                continue;
            }

            // Réponse à la commande unitaire : "DIST <cm>"
            if (line.startsWith("DIST ")) {
                bool ok = false;
                double dist = line.mid(5).trimmed().toDouble(&ok);
                if (ok) {
                    if (dist < 0.0) {
                        ui->label_Distance->setText("Hors portée");
                        appendTableRow(currentAngle, "Hors portée");
                    } else {
                        QString distStr = QString::number(dist, 'f', 1);
                        ui->label_Distance->setText(distStr + " cm");
                        ui->radarWidget->addMeasurement(currentAngle, dist);
                        appendTableRow(currentAngle, distStr);
                    }
                    setStatus("Mesure unitaire reçue à " + QString::number(currentAngle) + "°.");
                }
                continue;
            }

            // Fin de trame de balayage : "END"
            if (line == "END") {
                setStatus("Balayage SCAN terminé (" +
                          QString::number(ui->tableWidget_Mesures->rowCount()) + " mesures reçues).");
                continue;
            }

            // Ligne d'un balayage SCAN : "<angle> <cm>"
            const QStringList parts = line.split(' ', Qt::SkipEmptyParts);
            if (parts.size() == 2) {
                bool okAngle = false, okDist = false;
                int ang = parts[0].toInt(&okAngle);
                double dist = parts[1].toDouble(&okDist);

                if (okAngle && okDist) {
                    currentAngle = ang;
                    ui->label_Angle->setText(QString::number(ang) + " °");
                    ui->radarWidget->setSweepAngle(ang);

                    if (dist < 0.0) {
                        ui->label_Distance->setText("Hors portée");
                        appendTableRow(ang, "TIMEOUT");
                    } else {
                        QString distStr = QString::number(dist, 'f', 1);
                        ui->label_Distance->setText(distStr + " cm");
                        ui->radarWidget->addMeasurement(ang, dist);
                        appendTableRow(ang, distStr);
                    }
                }
            }
        }
    }
}

void Dialog::on_btn_ExportCsv_clicked() {
    if (ui->tableWidget_Mesures->rowCount() == 0) {
        setStatus("Aucune mesure à exporter.", true);
        return;
    }

    QString fileName = QFileDialog::getSaveFileName(
        this, "Exporter les mesures radar", "scan_radar.csv", "Fichiers CSV (*.csv)");
    if (fileName.isEmpty()) {
        return;
    }

    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        setStatus("Impossible d'écrire le fichier CSV.", true);
        return;
    }

    QTextStream out(&file);
    out << "Angle_deg;Distance_cm\n";
    for (int r = 0; r < ui->tableWidget_Mesures->rowCount(); ++r) {
        out << ui->tableWidget_Mesures->item(r, 0)->text() << ";"
            << ui->tableWidget_Mesures->item(r, 1)->text() << "\n";
    }
    file.close();
    setStatus("Export CSV réussi : " + fileName);
}