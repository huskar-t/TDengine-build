#include <stdio.h>
#include <stdlib.h>
#include <semaphore.h>
#include <errno.h>
#include <string.h>
#include <time.h>
#include <stdlib.h>
#include <inttypes.h>
#include "taos.h"
#include "tools.h"

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
    TAOS *taos = taos_connect(host, user, passwd, db, port);
    if (taos == NULL)
    {
        fprintf(stderr, "Failed to connect to %s:%hu, ErrCode: 0x%x, ErrMessage: %s.\n", host, port, taos_errno(NULL),
                taos_errstr(NULL));
        taos_cleanup();
        return -1;
    }
    int code = execute_query(taos, "create database if not exists test WAL_RETENTION_PERIOD 0", false, 0);
    if (code != 0)
    {
        taos_close(taos);
        exit(1);
    }
    code = execute_query(taos, "use test", false, 0);
    if (code != 0)
    {
        taos_close(taos);
        exit(1);
    }
    char *create_table_sql = "create table if not exists all_type(ts timestamp,"
                             "c1 bool,"
                             "c2 tinyint,"
                             "c3 smallint,"
                             "c4 int,"
                             "c5 bigint,"
                             "c6 tinyint unsigned,"
                             "c7 smallint unsigned,"
                             "c8 int unsigned,"
                             "c9 bigint unsigned,"
                             "c10 float,"
                             "c11 double,"
                             "c12 binary(20),"
                             "c13 nchar(20)"
                             ")"
                             "tags(tts timestamp,"
                             "tc1 bool,"
                             "tc2 tinyint,"
                             "tc3 smallint,"
                             "tc4 int,"
                             "tc5 bigint,"
                             "tc6 tinyint unsigned,"
                             "tc7 smallint unsigned,"
                             "tc8 int unsigned,"
                             "tc9 bigint unsigned,"
                             "tc10 float,"
                             "tc11 double,"
                             "tc12 binary(20),"
                             "tc13 nchar(20)"
                             ")";
    code = execute_query(taos, create_table_sql, false, 0);
    if (code != 0)
    {
        taos_close(taos);
        exit(1);
    }
    sem_t sem;
    code = sem_init(&sem, 0, 0);
    if (code != 0)
    {
        fprintf(stderr, "Failed to initialize semaphore, ErrCode: %d, ErrMessage: %s.\n", errno, strerror(errno));
        taos_close(taos);
        exit(1);
    }
    // get current timestamp ms
    struct timespec ts_now;
    clock_gettime(CLOCK_REALTIME, &ts_now);
    // get current timestamp ms
    int64_t ts_now_ms = ts_now.tv_sec * 1000 + ts_now.tv_nsec / 1000000;
    char insert_sql[512];
    sprintf(insert_sql,
            "insert into t1 using all_type tags(%" PRId64 ", true, 1, 2, 3, 4, 5, 6, 7, 8, 9.0, 10.0, 'hello', 'world') "
            "values(%" PRId64 ", true, 1, 2, 3, 4, 5, 6, 7, 8, 9.0, 10.0, 'hello', 'world')",
            ts_now_ms, ts_now_ms);
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
        code = execute_query(taos, insert_sql, false, 0);
        if (code != 0)
        {
            fprintf(stderr, "Failed to execute insert\n");
            taos_close(taos);
            exit(1);
        }
        code = execute_async(taos, insert_sql, false, &sem, 0);
        if (code != 0)
        {
            fprintf(stderr, "Failed to execute async insert\n");
            taos_close(taos);
            exit(1);
        }
        code = execute_query(taos, insert_sql, false, 0xabcd);
        if (code != 0)
        {
            fprintf(stderr, "Failed to execute insert with reqid\n");
            taos_close(taos);
            exit(1);
        }
        code = execute_async(taos, insert_sql, false, &sem, 0xabcde);
        if (code != 0)
        {
            fprintf(stderr, "Failed to execute async insert with reqid\n");
            taos_close(taos);
            exit(1);
        }
    }
    taos_close(taos);
}