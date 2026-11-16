#include "dialog.h"

#include <QApplication>
#include <QMessageBox>

#include "udp_client.h"

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    // Winsock doit être initialisé une fois, avant tout appel réseau.
    if (udp_global_init() != UDP_OK) {
        QMessageBox::critical(nullptr, QObject::tr("Erreur réseau"),
                              QObject::tr("Impossible d'initialiser Winsock (WSAStartup, code %1).")
                                  .arg(udp_last_wsa_error()));
        return 1;
    }

    int ret;
    {
        Dialog w;          // détruit (thread réseau arrêté) avant WSACleanup
        w.show();
        ret = QApplication::exec();
    }

    udp_global_cleanup();
    return ret;
}
