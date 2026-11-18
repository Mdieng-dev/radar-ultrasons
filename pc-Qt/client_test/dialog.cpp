#include "dialog.h"
#include "./ui_dialog.h"

#include <QDateTime>
#include <QHeaderView>
#include <QPushButton>
#include <QSettings>
#include <QTableWidgetItem>

#include "udp_client.h"   // UDP_OK, UDP_ERR_CANCEL, RADAR_ERR_BOARD

Dialog::Dialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::Dialog)
{
    ui->setupUi(this);

    qRegisterMetaType<RadarRequest>();
    qRegisterMetaType<RadarResult>();

    // Entrée dans un champ ne doit pas déclencher un bouton par défaut.
    for (QPushButton *b : findChildren<QPushButton *>()) {
        b->setAutoDefault(false);
        b->setDefault(false);
    }

    ui->tableScan->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    ui->tableScan->verticalHeader()->setVisible(false);
    ui->progressBusy->setRange(0, 1);
    ui->progressBusy->setValue(0);

    // --- Thread réseau : le worker vit dans m_thread ---
    m_worker = new Radar_Thread;                 // pas de parent : il va changer de thread
    m_worker->moveToThread(&m_thread);
    connect(&m_thread, &QThread::finished, m_worker, &QObject::deleteLater);
    connect(this, &Dialog::radarRequested, m_worker, &Radar_Thread::process);
    connect(m_worker, &Radar_Thread::attempt, this, &Dialog::onAttempt);
    connect(m_worker, &Radar_Thread::finished, this, &Dialog::onFinished);
    m_thread.setObjectName(QStringLiteral("reseau"));
    m_thread.start();

    // --- Boutons ---
    connect(ui->buttonAngle, &QPushButton::clicked, this, &Dialog::onAngleClicked);
    connect(ui->buttonDist,  &QPushButton::clicked, this, &Dialog::onDistClicked);
    connect(ui->buttonScan,  &QPushButton::clicked, this, &Dialog::onScanClicked);
    connect(ui->buttonCancel, &QPushButton::clicked, this, &Dialog::onCancelClicked);
    connect(ui->buttonClearLog, &QPushButton::clicked, ui->plainTextLog, &QPlainTextEdit::clear);

    loadSettings();
}

Dialog::~Dialog()
{
    saveSettings();
    // Fermeture propre : on interrompt une éventuelle attente réseau (le
    // module C vérifie l'annulation toutes les 100 ms, et ferme son socket),
    // puis on arrête le thread et on attend sa fin.
    m_worker->requestCancel();
    m_thread.quit();
    m_thread.wait();
    delete ui;
}

/*------------------------------------------------------------------------------
 * Construction et validation d'une requête à partir des champs saisis
 *----------------------------------------------------------------------------*/
bool Dialog::buildRequest(RadarRequest::Kind kind, RadarRequest &req)
{
    const QString host = ui->lineEditHost->text().trimmed();
    if (host.isEmpty()) {
        showError(tr("Saisissez l'adresse du routeur (adresse IP WAN)."));
        ui->lineEditHost->setFocus();
        return false;
    }
    if (host.contains(QLatin1Char(' ')) || host.toLatin1() != host.toUtf8()) {
        showError(tr("Adresse invalide : « %1 ».").arg(host));
        ui->lineEditHost->setFocus();
        return false;
    }

    req.kind      = kind;
    req.host      = host;
    req.port      = static_cast<quint16>(ui->spinPort->value());
    req.retries   = ui->spinRetries->value();
    req.timeoutMs = (kind == RadarRequest::Scan) ? ui->spinScanTimeout->value()
                                                 : ui->spinTimeout->value();
    req.angle     = ui->spinAngle->value();
    req.start     = ui->spinScanStart->value();
    req.end       = ui->spinScanEnd->value();
    req.step      = ui->spinScanStep->value();
    return true;
}

void Dialog::startRequest(const RadarRequest &req, const QString &command)
{
    if (m_busy)
        return;                                 // une seule requête à la fois
    log(QStringLiteral("→ %1  (%2:%3)").arg(command, req.host).arg(req.port));
    setBusy(true);
    m_worker->resetCancel();
    emit radarRequested(req);                   // retour immédiat : rien ne bloque
}

/*------------------------------------------------------------------------------
 * Commandes
 *----------------------------------------------------------------------------*/
void Dialog::onAngleClicked()
{
    RadarRequest req;
    if (buildRequest(RadarRequest::Angle, req))
        startRequest(req, QStringLiteral("ANGLE %1").arg(req.angle));
}

void Dialog::onDistClicked()
{
    RadarRequest req;
    if (buildRequest(RadarRequest::Dist, req))
        startRequest(req, QStringLiteral("DIST"));
}

void Dialog::onScanClicked()
{
    RadarRequest req;
    if (!buildRequest(RadarRequest::Scan, req))
        return;
    if (req.start > req.end) {
        showError(tr("Le début du balayage doit être inférieur ou égal à la fin."));
        return;
    }
    startRequest(req, QStringLiteral("SCAN %1 %2 %3").arg(req.start).arg(req.end).arg(req.step));
}

void Dialog::onCancelClicked()
{
    if (m_busy) {
        m_worker->requestCancel();
        ui->labelStatus->setText(tr("Annulation…"));
    }
}

/*------------------------------------------------------------------------------
 * Retours du worker (exécutés dans le thread de l'interface)
 *----------------------------------------------------------------------------*/
void Dialog::onAttempt(int attempt, int maxAttempts)
{
    ui->labelStatus->setText(tr("En attente de la réponse… essai %1/%2")
                                 .arg(attempt).arg(maxAttempts));
    if (attempt > 1)
        log(tr("   pas de réponse, nouvel essai %1/%2").arg(attempt).arg(maxAttempts));
}

void Dialog::onFinished(const RadarResult &res)
{
    setBusy(false);

    // Réponse brute (sur une ligne, \n rendu visible)
    if (!res.raw.isEmpty()) {
        QString shown = res.raw.trimmed();
        shown.replace(QLatin1Char('\n'), QStringLiteral(" | "));
        log(QStringLiteral("← %1  (%2 ms)").arg(shown).arg(res.elapsedMs));
    }

    if (res.status == UDP_ERR_CANCEL) {
        ui->labelStatus->setText(tr("Commande annulée."));
        log(tr("   annulée"));
        return;
    }
    if (res.status != UDP_OK) {
        QString msg = res.errorText;
        if (res.status == RADAR_ERR_BOARD)
            msg += QStringLiteral(" : ") + res.raw.trimmed();
        showError(msg);
        return;
    }

    switch (res.request.kind) {
    case RadarRequest::Angle:
        ui->labelStatus->setText(tr("Servo orienté à %1°.").arg(res.request.angle));
        break;

    case RadarRequest::Dist:
        if (res.noEcho) {
            ui->labelDistance->setText(tr("Distance : pas d'écho"));
            ui->labelStatus->setText(tr("Mesure : aucun écho (TIMEOUT)."));
        } else {
            ui->labelDistance->setText(tr("Distance : %1 cm").arg(res.cm, 0, 'f', 1));
            ui->labelStatus->setText(tr("Mesure reçue."));
        }
        break;

    case RadarRequest::Scan: {
        QTableWidget *t = ui->tableScan;
        t->setRowCount(0);
        t->setRowCount(res.points.size());
        for (int i = 0; i < res.points.size(); ++i) {
            const ScanPoint &p = res.points.at(i);
            auto *a = new QTableWidgetItem(QString::number(p.angle));
            auto *d = new QTableWidgetItem(p.noEcho ? tr("pas d'écho")
                                                    : QString::number(p.cm, 'f', 1));
            a->setTextAlignment(Qt::AlignCenter);
            d->setTextAlignment(Qt::AlignCenter);
            t->setItem(i, 0, a);
            t->setItem(i, 1, d);
        }
        ui->labelStatus->setText(tr("Balayage terminé : %1 mesure(s).").arg(res.points.size()));
        break;
    }
    }
}

/*------------------------------------------------------------------------------
 * Utilitaires d'affichage
 *----------------------------------------------------------------------------*/
void Dialog::setBusy(bool busy)
{
    m_busy = busy;
    ui->buttonAngle->setEnabled(!busy);
    ui->buttonDist->setEnabled(!busy);
    ui->buttonScan->setEnabled(!busy);
    ui->groupConnexion->setEnabled(!busy);
    ui->buttonCancel->setEnabled(busy);
    // Barre « indéterminée » (min = max = 0) : elle s'anime tant que la boucle
    // d'événements tourne, preuve visuelle que l'interface ne se fige pas.
    ui->progressBusy->setRange(0, busy ? 0 : 1);
    ui->progressBusy->setValue(0);
    if (busy)
        ui->labelStatus->setStyleSheet(QString());
}

void Dialog::showError(const QString &text)
{
    ui->labelStatus->setStyleSheet(QStringLiteral("color: #c62828; font-weight: bold;"));
    ui->labelStatus->setText(tr("Erreur : %1").arg(text));
    log(tr("   ERREUR : %1").arg(text));
}

void Dialog::log(const QString &text)
{
    const QString ts = QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss.zzz"));
    ui->plainTextLog->appendPlainText(ts + QStringLiteral("  ") + text);
}

void Dialog::loadSettings()
{
    QSettings s(QStringLiteral("radar-ultrasons"), QStringLiteral("client_test"));
    ui->lineEditHost->setText(s.value(QStringLiteral("host")).toString());
    ui->spinPort->setValue(s.value(QStringLiteral("port"), 8000).toInt());
    ui->spinTimeout->setValue(s.value(QStringLiteral("timeoutMs"), 1000).toInt());
    ui->spinRetries->setValue(s.value(QStringLiteral("retries"), 2).toInt());
    ui->spinScanTimeout->setValue(s.value(QStringLiteral("scanTimeoutMs"), 10000).toInt());
}

void Dialog::saveSettings() const
{
    QSettings s(QStringLiteral("radar-ultrasons"), QStringLiteral("client_test"));
    s.setValue(QStringLiteral("host"), ui->lineEditHost->text().trimmed());
    s.setValue(QStringLiteral("port"), ui->spinPort->value());
    s.setValue(QStringLiteral("timeoutMs"), ui->spinTimeout->value());
    s.setValue(QStringLiteral("retries"), ui->spinRetries->value());
    s.setValue(QStringLiteral("scanTimeoutMs"), ui->spinScanTimeout->value());
}
