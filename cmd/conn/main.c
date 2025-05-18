#include <stdio.h>
#include "taos.h"

int main() {
    const char *host = "localhost";
    const char *user = "root";
    const char *passwd = "taosdata";
    const char *db = NULL;
    uint16_t    port = 6030;
    while(1){
        TAOS       *taos = taos_connect(host, user, passwd, db, port);
        if (taos == NULL) {
            fprintf(stderr, "Failed to connect to %s:%hu, ErrCode: 0x%x, ErrMessage: %s.\n", host, port, taos_errno(NULL), taos_errstr(NULL));
            taos_cleanup();
            return -1;
        }
        taos_close(taos);
    }
}