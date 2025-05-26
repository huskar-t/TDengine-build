#include <stdio.h>
#include <sys/time.h>
#include <stdlib.h>
#include "tools.h"
#include "taos.h"

int main(int argc, char *argv[])
{
    int64_t timeout_seconds = -1; // -1 means infinite loop
    if (argc > 1)
    {
        timeout_seconds = atoll(argv[1]);
        printf("Timeout set to %ld seconds.\n", timeout_seconds);
    }
    int64_t start_time = get_time_seconds();
    int64_t current_time = start_time;
    printf("start_time: %ld\n", start_time);
    const char *host = "localhost";
    const char *user = "root";
    const char *passwd = "taosdata";
    const char *db = NULL;
    uint16_t port = 6030;
    while (1)
    {
        if (timeout_seconds > 0)
        {
            current_time = get_time_seconds();
            if (current_time - start_time >= timeout_seconds)
            {
                printf("Timeout reached, end time: %ld\n", current_time);
                break;
            }
        }
        TAOS *taos = taos_connect(host, user, passwd, db, port);
        if (taos == NULL)
        {
            fprintf(stderr, "Failed to connect to %s:%hu, ErrCode: 0x%x, ErrMessage: %s.\n", host, port, taos_errno(NULL), taos_errstr(NULL));
            taos_cleanup();
            return -1;
        }
        taos_close(taos);
    }
}