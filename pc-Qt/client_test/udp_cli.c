/*------------------------------------------------------------------------------
 * udp_cli.c — Client console pour tester le module réseau SANS interface Qt.
 *
 * Usage :
 *   udp_cli <hote> <port> ANGLE <a>
 *   udp_cli <hote> <port> DIST
 *   udp_cli <hote> <port> SCAN <debut> <fin> <pas>
 *   udp_cli --selftest                 (tests des analyseurs, sans réseau)
 *
 * Options (avant <hote>) : -t <délai ms> (défaut 1000), -r <essais> (défaut 2)
 *----------------------------------------------------------------------------*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "udp_client.h"
#include "radar_proto.h"

static void on_attempt(void *ctx, int attempt, int max_attempts)
{
    (void)ctx;
    fprintf(stderr, "[essai %d/%d]\n", attempt, max_attempts);
}

/*---------------------------- Auto-tests ------------------------------------*/
static int g_fail = 0;
#define CHECK(cond) do { if (!(cond)) { printf("ECHEC ligne %d : %s\n", __LINE__, #cond); ++g_fail; } } while (0)

static int selftest(void)
{
    radar_point_t pts[RADAR_MAX_POINTS];
    double cm; int ne, n;

    CHECK(radar_parse_angle_reply("OK\n") == UDP_OK);
    CHECK(radar_parse_angle_reply("ERR RANGE\n") == RADAR_ERR_BOARD);
    CHECK(radar_parse_angle_reply("BLA\n") == RADAR_ERR_PROTOCOL);

    CHECK(radar_parse_dist_reply("DIST 23.5\n", &cm, &ne) == UDP_OK && !ne && cm > 23.49 && cm < 23.51);
    CHECK(radar_parse_dist_reply("DIST TIMEOUT\n", &cm, &ne) == UDP_OK && ne);
    CHECK(radar_parse_dist_reply("DIST 2x\n", &cm, &ne) == RADAR_ERR_PROTOCOL);
    CHECK(radar_parse_dist_reply("ERR UNKNOWN\n", &cm, &ne) == RADAR_ERR_BOARD);

    CHECK(radar_parse_scan_reply("0 12.3\n10 TIMEOUT\r\n20 40.0\nEND\n", pts, RADAR_MAX_POINTS, &n) == UDP_OK);
    CHECK(n == 3 && pts[0].angle == 0 && pts[1].no_echo && pts[2].cm > 39.9);
    CHECK(radar_parse_scan_reply("0 12.3\n10 15.0\n", pts, RADAR_MAX_POINTS, &n) == RADAR_ERR_PROTOCOL); /* END absent */
    CHECK(radar_parse_scan_reply("ERR RANGE\n", pts, RADAR_MAX_POINTS, &n) == RADAR_ERR_BOARD);
    CHECK(radar_parse_scan_reply("0 1.0 9\nEND\n", pts, RADAR_MAX_POINTS, &n) == RADAR_ERR_PROTOCOL);
    CHECK(radar_parse_scan_reply("0 1.0\n10 2.0\nEND\n", pts, 1, &n) == RADAR_ERR_PROTOCOL);    /* trop de points */

    printf(g_fail ? "%d test(s) en echec\n" : "Tous les tests passent\n", g_fail);
    return g_fail ? 1 : 0;
}

/*----------------------------------------------------------------------------*/
static int usage(void)
{
    fprintf(stderr,
        "Usage : udp_cli [-t ms] [-r essais] <hote> <port> ANGLE <a> | DIST | SCAN <d> <f> <p>\n"
        "        udp_cli --selftest\n");
    return 2;
}

int main(int argc, char **argv)
{
    udp_hooks_t hooks = { NULL, on_attempt, NULL };
    radar_conn_t c;
    char raw[RADAR_REPLY_SIZE] = "";
    int i = 1, st = UDP_ERR_PARAM, timeout = 1000, retries = 2;

    if (argc == 2 && strcmp(argv[1], "--selftest") == 0)
        return selftest();

    while (i + 1 < argc && argv[i][0] == '-') {
        if (strcmp(argv[i], "-t") == 0)      timeout = atoi(argv[i + 1]);
        else if (strcmp(argv[i], "-r") == 0) retries = atoi(argv[i + 1]);
        else return usage();
        i += 2;
    }
    if (argc - i < 3) return usage();

    if (udp_global_init() != UDP_OK) {
        fprintf(stderr, "WSAStartup a echoue\n");
        return 1;
    }

    c.host = argv[i];
    c.port = (unsigned short)atoi(argv[i + 1]);
    c.timeout_ms = timeout;
    c.retries = retries;
    c.hooks = &hooks;
    i += 2;

    if (strcmp(argv[i], "ANGLE") == 0 && argc - i == 2) {
        st = radar_angle(&c, atoi(argv[i + 1]), raw, sizeof raw);
        if (st == UDP_OK) printf("OK\n");
    } else if (strcmp(argv[i], "DIST") == 0 && argc - i == 1) {
        double cm; int ne;
        st = radar_dist(&c, &cm, &ne, raw, sizeof raw);
        if (st == UDP_OK) {
            if (ne) printf("Pas d'echo (TIMEOUT)\n");
            else    printf("Distance : %.1f cm\n", cm);
        }
    } else if (strcmp(argv[i], "SCAN") == 0 && argc - i == 4) {
        radar_point_t pts[RADAR_MAX_POINTS];
        int n, k;
        st = radar_scan(&c, atoi(argv[i + 1]), atoi(argv[i + 2]), atoi(argv[i + 3]),
                        pts, RADAR_MAX_POINTS, &n, raw, sizeof raw);
        if (st == UDP_OK) {
            printf("Angle  Distance\n");
            for (k = 0; k < n; ++k) {
                if (pts[k].no_echo) printf("%5d  TIMEOUT\n", pts[k].angle);
                else                printf("%5d  %.1f cm\n", pts[k].angle, pts[k].cm);
            }
        }
    } else {
        udp_global_cleanup();
        return usage();
    }

    if (st != UDP_OK) {
        fprintf(stderr, "Erreur : %s", radar_strerror(st));
        if (udp_last_wsa_error()) fprintf(stderr, " (WSA %d)", udp_last_wsa_error());
        if (raw[0]) fprintf(stderr, " - reponse : %s", raw);
        fprintf(stderr, "\n");
    }
    udp_global_cleanup();
    return st == UDP_OK ? 0 : 1;
}
