#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>
#include <stdlib.h>
#include "taos.h"
#include "tools.h"

void stmtAsyncQueryCb(void *param, TAOS_RES *pRes, int code) { sleep(1); }

int initEnv(TAOS *taos)
{
    int code = execute_query(taos, "create database if not exists db WAL_RETENTION_PERIOD 0", false, 0);
    if (code != 0)
    {
        printf("failed to execute create database. error:%s\n", taos_errstr(taos));
        return code;
    }
    code = execute_query(taos, "create table if not exists db.stb (ts timestamp, b binary(10)) tags(t1 int, t2 binary(10))", false, 0);
    if (code != 0)
    {
        printf("failed to execute create table. error:%s\n", taos_errstr(taos));
        return code;
    }
    code = execute_query(taos, "use db", false, 0);
    if (code != 0)
    {
        printf("failed to execute use database. error:%s\n", taos_errstr(taos));
        return code;
    }
    return 0;
}

int do_stmt(TAOS *taos)
{

    TAOS_STMT2_OPTION option = {0, true, true, NULL, NULL};

    TAOS_STMT2 *stmt = taos_stmt2_init(taos, &option);

    int code = taos_stmt2_prepare(stmt, "select * from db.stb where ts > ?", 0);
    if (code != 0)
    {
        printf("failed to execute taos_stmt2_prepare. error:%s\n", taos_stmt2_error(stmt));
        taos_stmt2_close(stmt);
        return code;
    }
    int is_insert = 0;
    code = taos_stmt2_is_insert(stmt, &is_insert);
    if (code != 0)
    {
        printf("failed to execute taos_stmt2_is_insert. error:%s\n", taos_stmt2_error(stmt));
        taos_stmt2_close(stmt);
        return code;
    }
    if (is_insert == 1)
    {
        printf("The statement is an insert statement, which is not expected in this example.\n");
        taos_stmt2_close(stmt);
        return -1;
    }
    int fieldNum = 0;
    TAOS_FIELD_ALL *pFields = NULL;
    code = taos_stmt2_get_fields(stmt, &fieldNum, &pFields);
    if (code != 0)
    {
        printf("failed get col,ErrCode: 0x%x, ErrMessage: %s.\n", code, taos_stmt2_error(stmt));
        return code;
    }
    taos_stmt2_free_fields(stmt, pFields);
    for (size_t i = 0; i < 30; i++)
    {

        int64_t last_3_secondes = get_time_ms() - 3000; // 3 seconds ago
        // bind params
        TAOS_STMT2_BIND paramv[1];
        paramv[0] = (TAOS_STMT2_BIND){TSDB_DATA_TYPE_TIMESTAMP, &last_3_secondes, NULL, NULL, 1};
        TAOS_STMT2_BIND *tbs[1] = {&paramv[0]};
        TAOS_STMT2_BINDV bindv = {1, NULL, NULL, &tbs[0]};
        code = taos_stmt2_bind_param(stmt, &bindv, -1);
        if (code != 0)
        {
            printf("failed to execute taos_stmt2_bind_param statement.ErrCode: 0x%x, ErrMessage: %s\n", code, taos_stmt2_error(stmt));
            taos_stmt2_close(stmt);
            return code;
        }
        code = taos_stmt2_exec(stmt, NULL);
        if (code != 0)
        {
            printf("failed to execute taos_stmt2_exec statement.error:%s\n", taos_stmt2_error(stmt));
            taos_stmt2_close(stmt);
            return code;
        }
        TAOS_RES *res = taos_stmt2_result(stmt);
        if (!res)
        {
            printf("failed to get result from taos_stmt2_result statement.error:%s\n", taos_stmt2_error(stmt));
            taos_stmt2_close(stmt);
            return -1;
        }
        int num_fields = taos_num_fields(res);
        if (num_fields <= 0)
        {
            printf("No fields returned from the query.\n");
            taos_stmt2_close(stmt);
            return -1;
        }
        TAOS_FIELD *fields = taos_fetch_fields(res);
        if (!fields)
        {
            printf("failed to fetch fields from result, error:%s\n", taos_errstr(taos));
            taos_stmt2_close(stmt);
            return -1;
        }
    }
    taos_stmt2_close(stmt);
    return 0;
}

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
    TAOS *taos = taos_connect("localhost", "root", "taosdata", "", 0);
    if (!taos)
    {
        printf("failed to connect to db, reason:%s\n", taos_errstr(taos));
        exit(1);
    }
    int code = initEnv(taos);
    if (code != 0)
    {
        taos_close(taos);
        exit(1);
    }
    int64_t ts_now_ms = get_time_ms();
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
        code = do_stmt(taos);
        if (code != 0)
        {
            printf("Failed to execute async statement1: %d\n", code);
            exit(1);
        }
    }
    taos_close(taos);
    taos_cleanup();
}