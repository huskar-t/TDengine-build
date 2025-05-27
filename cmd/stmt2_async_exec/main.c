#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/time.h>
#include "taos.h"
#include "tools.h"

int CTB_NUMS = 10;
int ROW_NUMS = 100;
int CYC_NUMS = 10;

void stmtAsyncQueryCb(void *param, TAOS_RES *pRes, int code) { sleep(1); }

int initEnv(TAOS *taos)
{
    int code = execute_query(taos, "create database if not exists db_async WAL_RETENTION_PERIOD 0", false, 0);
    if (code != 0)
    {
        printf("failed to execute create database. error:%s\n", taos_errstr(taos));
        return code;
    }
    code = execute_query(taos, "create table if not exists db_async.stb (ts timestamp, b binary(10)) tags(t1 int, t2 binary(10))", false, 0);
    if (code != 0)
    {
        printf("failed to execute create table. error:%s\n", taos_errstr(taos));
        return code;
    }
    code = execute_query(taos, "use db_async", false, 0);
    if (code != 0)
    {
        printf("failed to execute use database. error:%s\n", taos_errstr(taos));
        return code;
    }
}

typedef struct AsyncExecParam
{
    sem_t *sem;
    TAOS_RES *result;
    int code;
    char *errmsg;
    int affected_rows;
} AsyncExecParam;

void async_exec_callback(void *param, TAOS_RES *res, int code)
{
    AsyncExecParam *async_param = (AsyncExecParam *)param;
    async_param->result = res;
    if (code != 0)
    {
        async_param->code = code;
        async_param->errmsg = strdup(taos_errstr(res));
        async_param->affected_rows = 0;
    }
    else
    {
        async_param->code = 0;
        async_param->affected_rows = taos_affected_rows(res);
    }
    sem_post(async_param->sem);
}

int do_stmt_async(TAOS *taos, const char *sql, bool hasTag, int64_t ts_now_ms, AsyncExecParam *param)
{

    TAOS_STMT2_OPTION option = {0, true, true, async_exec_callback, param};

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

        int ret = 0;
        do
        {
            ret = sem_wait(param->sem);
        } while (-1 == ret && errno == EINTR);
        if (ret != 0)
        {
            fprintf(stderr, "Failed to wait for stmt2 semaphore\n");
            return ret;
        }
        if (param->code != 0)
        {
            fprintf(stderr, "stmt2 exec failed: %d:%s\n", param->code, param->errmsg);
            taos_stmt2_close(stmt);
            return param->code;
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
    // get current timestamp ms
    struct timeval tv;
    gettimeofday(&tv, NULL);
    int64_t ts_now_ms = tv.tv_sec * 1000 + tv.tv_usec / 1000;
    AsyncExecParam *param = (AsyncExecParam *)malloc(sizeof(AsyncExecParam));
    sem_t sem;
    sem_init(&sem, 0, 0);
    param->sem = &sem;
    param->result = NULL;
    param->code = 0;
    param->errmsg = NULL;
    param->affected_rows = 0;
    while (1)
    // for (int i = 0; i < 10; i++)
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
        code = do_stmt_async(taos, "insert into db_async.? using db_async.stb tags(?,?)values(?,?)", true, ts_now_ms, param);
        if (code != 0)
        {
            fprintf(stderr, "Failed to execute async statement1: %d:%s\n", code, param->errmsg);
            exit(1);
        }
        code = do_stmt_async(taos, "insert into ? using stb tags(?,?)values(?,?)", true, ts_now_ms, param);
        if (code != 0)
        {
            fprintf(stderr, "Failed to execute async statement2: %d:%s\n", code, param->errmsg);
            exit(1);
        }
        code = do_stmt_async(taos, "insert into stb (tbname,ts,b,t1,t2) values(?,?,?,?,?)", true, ts_now_ms, param);
        if (code != 0)
        {
            fprintf(stderr, "Failed to execute async statement3: %d:%s\n", code, param->errmsg);
            exit(1);
        }
    }
    taos_close(taos);
    taos_cleanup();
}