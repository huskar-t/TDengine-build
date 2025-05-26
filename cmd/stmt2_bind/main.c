#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include "taos.h"
#include "tools.h"

int CTB_NUMS = 10;
int ROW_NUMS = 100;
int CYC_NUMS = 10;

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
}

int do_stmt(TAOS *taos, const char *sql, bool hasTag, int64_t ts_now_ms)
{

    TAOS_STMT2_OPTION option = {0, true, true, NULL, NULL};

    TAOS_STMT2 *stmt = taos_stmt2_init(taos, &option);

    // tbname
    char **tbs = (char **)malloc(CTB_NUMS * sizeof(char *));
    for (int i = 0; i < CTB_NUMS; i++)
    {
        tbs[i] = (char *)malloc(sizeof(char) * 20);
        sprintf(tbs[i], "ctb_%d", i);
    }
    int code = 0;

    double bind_time = 0;
    double exec_time = 0;
    for (int r = 0; r < CYC_NUMS; r++)
    {
        int code = taos_stmt2_prepare(stmt, sql, 0);
        if (code != 0)
        {
            printf("failed to execute taos_stmt2_prepare. error:%s\n", taos_stmt2_error(stmt));
            taos_stmt2_close(stmt);
            return code;
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
        // col params
        int64_t **ts = (int64_t **)malloc(CTB_NUMS * sizeof(int64_t *));
        char **b = (char **)malloc(CTB_NUMS * sizeof(char *));
        int *ts_len = (int *)malloc(ROW_NUMS * sizeof(int));
        int *b_len = (int *)malloc(ROW_NUMS * sizeof(int));
        for (int i = 0; i < ROW_NUMS; i++)
        {
            ts_len[i] = sizeof(int64_t);
            b_len[i] = 1;
        }

        for (int i = 0; i < CTB_NUMS; i++)
        {
            ts[i] = (int64_t *)malloc(ROW_NUMS * sizeof(int64_t));
            b[i] = (char *)malloc(ROW_NUMS * sizeof(char));
            for (int j = 0; j < ROW_NUMS; j++)
            {
                ts[i][j] = ts_now_ms + r * CYC_NUMS + j;
                b[i][j] = 'a' + j;
            }
        }
        // tag params
        int t1 = 0;
        int t1len = sizeof(int);
        int t2len = 3;

        // bind params
        TAOS_STMT2_BIND **paramv = (TAOS_STMT2_BIND **)malloc(CTB_NUMS * sizeof(TAOS_STMT2_BIND *));
        TAOS_STMT2_BIND **tags = (TAOS_STMT2_BIND **)malloc(CTB_NUMS * sizeof(TAOS_STMT2_BIND *));
        for (int i = 0; i < CTB_NUMS; i++)
        {
            // create tags
            tags[i] = (TAOS_STMT2_BIND *)malloc(2 * sizeof(TAOS_STMT2_BIND));
            tags[i][0] = (TAOS_STMT2_BIND){TSDB_DATA_TYPE_INT, &t1, &t1len, NULL, 0};
            tags[i][1] = (TAOS_STMT2_BIND){TSDB_DATA_TYPE_BINARY, "after", &t2len, NULL, 0};

            // create col params
            paramv[i] = (TAOS_STMT2_BIND *)malloc(2 * sizeof(TAOS_STMT2_BIND));
            paramv[i][0] = (TAOS_STMT2_BIND){TSDB_DATA_TYPE_TIMESTAMP, &ts[i][0], &ts_len[0], NULL, ROW_NUMS};
            paramv[i][1] = (TAOS_STMT2_BIND){TSDB_DATA_TYPE_BINARY, &b[i][0], &b_len[0], NULL, ROW_NUMS};
        }
        // bind
        struct timespec start, end;
        clock_gettime(CLOCK_MONOTONIC, &start); // 获取开始时间
        TAOS_STMT2_BINDV bindv;
        if (hasTag)
        {
            bindv = (TAOS_STMT2_BINDV){CTB_NUMS, tbs, tags, paramv};
        }
        else
        {
            bindv = (TAOS_STMT2_BINDV){CTB_NUMS, tbs, NULL, paramv};
        }
        code = taos_stmt2_bind_param(stmt, &bindv, -1);
        if (code != 0)
        {
            printf("failed to execute taos_stmt2_bind_param statement.error:%s\n", taos_stmt2_error(stmt));
            taos_stmt2_close(stmt);
            return code;
        }

        // exec
        code = taos_stmt2_exec(stmt, NULL);
        if (code != 0)
        {
            printf("failed to execute taos_stmt2_exec statement.error:%s\n", taos_stmt2_error(stmt));
            taos_stmt2_close(stmt);
            return code;
        }

        for (int i = 0; i < CTB_NUMS; i++)
        {
            free(tags[i]);
            free(paramv[i]);
            free(ts[i]);
            free(b[i]);
        }
        free(ts);
        free(b);
        free(ts_len);
        free(b_len);
        free(paramv);
        free(tags);
    }

    for (int i = 0; i < CTB_NUMS; i++)
    {
        free(tbs[i]);
    }
    free(tbs);

    taos_stmt2_close(stmt);
}

int main()
{
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
    // get current timestamp ms
    struct timespec ts_now;
    clock_gettime(CLOCK_REALTIME, &ts_now);
    // get current timestamp ms
    int64_t ts_now_ms = ts_now.tv_sec * 1000 + ts_now.tv_nsec / 1000000;
    while (1)
    // for (int i = 0; i < 10; i++)
    {
        code = do_stmt(taos, "insert into db.? using db.stb tags(?,?)values(?,?)", true,ts_now_ms);
        if (code != 0)
        {
            exit(1);
        }
        code = do_stmt(taos, "insert into ? using stb tags(?,?)values(?,?)", true,ts_now_ms);
        if (code != 0)
        {
            exit(1);
        }
        code = do_stmt(taos, "insert into stb (tbname,ts,b,t1,t2) values(?,?,?,?,?)", true,ts_now_ms);
        if (code != 0)
        {
            exit(1);
        }
    }
    taos_close(taos);
    taos_cleanup();
}