#include <sys/resource.h>
#include <semaphore.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <malloc.h>
#include "tools.h"
#include "taos.h"

int execute_query(TAOS *taos, const char *query, bool fetch_result, int64_t reqid) {
    TAOS_RES *result;
    if (reqid == 0){
        result = taos_query(taos, query);
    }else{
        result = taos_query_with_reqid(taos,query,reqid);
    }
    if (result == NULL) {
        fprintf(stderr, "Query failed: %s\n", query);
        return -1;
    }
    int code = taos_errno(result);
    if (code != 0) {
        taos_free_result(result);
        fprintf(stderr, "Failed to exec sql, ErrCode: 0x%x, ErrMessage: %s,sql: %s.\n", code, taos_errstr(result),
                query);
        return -1;
    }
    int is_update_query = taos_is_update_query(result);
    if (is_update_query) {
        int affected_rows = taos_affected_rows(result);
        if (affected_rows < 0) {
            taos_free_result(result);
            fprintf(stderr, "Failed to get affected rows, affected_rows:%d,sql: %s.\n", affected_rows, query);
            return -1;
        }
        taos_free_result(result);
        return 0;
    } else {
        int num_fields = taos_num_fields(result);
        if (num_fields <= 0) {
            taos_free_result(result);
            fprintf(stderr, "No fields in result set,sql: %s.\n", query);
            return -1;
        }
        TAOS_FIELD_E *fields = taos_fetch_fields_e(result);
        if (fields == NULL) {
            taos_free_result(result);
            fprintf(stderr, "Failed to fetch fields, sql: %s.\n", query);
            return -1;
        }
        if (fetch_result) {
            int rows;
            void * data;
            while (1) {
                code = taos_fetch_raw_block(result,&rows,&data);
                if (code != 0) {
                    const char * errmsg = taos_errstr(result);
                    taos_free_result(result);
                    fprintf(stderr, "Failed to fetch data, code: %d, msg:%s , sql: %s.\n", code,errmsg, query);
                    return -1;
                }
                if (rows == 0) {
                    break;
                }
            }
        }
    }
    taos_free_result(result);
    return 0;
}

typedef struct AsyncQueryParam {
    sem_t *sem;
    TAOS_RES *result;
    int code;
    char *errmsg;
} AsyncQueryParam;

void async_query_callback(void *param, TAOS_RES *res, int code) {
    AsyncQueryParam *async_param = (AsyncQueryParam *) param;
    async_param->result = res;
    if (code != 0) {
        async_param->code = code;
        async_param->errmsg = strdup(taos_errstr(res));
    } else {
        async_param->code = 0;
    }
    sem_post(async_param->sem);
}

int execute_async(TAOS *taos, const char *query, bool fetch_result, sem_t *sem, int64_t reqid) {
    AsyncQueryParam *param = calloc(1, sizeof(AsyncQueryParam));
    if (param == NULL) {
        fprintf(stderr, "Failed to allocate memory for AsyncQueryParam\n");
        return -1;
    }
    param->sem = sem;
    if (reqid != 0) {
        taos_query_a_with_reqid(taos, query, async_query_callback, param, reqid);
    } else {
        taos_query_a(taos, query, async_query_callback, param);
    }
    int ret = 0;
    do {
        ret = sem_wait(sem);
    } while (-1 == ret && errno == EINTR);
    if (ret != 0) {
        fprintf(stderr, "Failed to wait for semaphore\n");
        free(param);
        return ret;
    }
    if (param->code != 0) {
        fprintf(stderr, "Async query failed: %s\n", param->errmsg);
        taos_free_result(param->result);
        free(param->errmsg);
        free(param);
        return -1;
    }
    int is_update_query = taos_is_update_query(param->result);
    if (is_update_query) {
        int affected_rows = taos_affected_rows(param->result);
        if (affected_rows < 0) {
            taos_free_result(param->result);
            free(param);
            fprintf(stderr, "Failed to get affected rows, affected_rows:%d,sql: %s.\n", affected_rows, query);
            return -1;
        }
        taos_free_result(param->result);
        free(param);
        return 0;
    } else {
        int num_fields = taos_num_fields(param->result);
        if (num_fields <= 0) {
            taos_free_result(param->result);
            free(param);
            fprintf(stderr, "No fields in param->result set,sql: %s.\n", query);
            return -1;
        }
        TAOS_FIELD_E *fields = taos_fetch_fields_e(param->result);
        if (fields == NULL) {
            taos_free_result(param->result);
            free(param);
            fprintf(stderr, "Failed to fetch fields, sql: %s.\n", query);
            return -1;
        }
        if (fetch_result) {
            while (1) {
                taos_fetch_raw_block_a(param->result, async_query_callback,param);
                do {
                    ret = sem_wait(sem);
                } while (-1 == ret && errno == EINTR);
                if (ret != 0) {
                    taos_free_result(param->result);
                    fprintf(stderr, "Failed to wait for semaphore\n");
                    free(param);
                    return ret;
                }
                if (param->code < 0) {
                    fprintf(stderr, "Async fetch failed: %s\n", param->errmsg);
                    taos_free_result(param->result);
                    free(param->errmsg);
                    free(param);
                    return -1;
                }
                if (param->code == 0) {
                    break;
                }
                const void * raw_block = taos_get_raw_block(param->result);
                if (raw_block == NULL) {
                    fprintf(stderr, "Failed to get raw block, sql: %s.\n", query);
                    taos_free_result(param->result);
                    free(param);
                    return -1;
                }
            }
        }
    }
    taos_free_result(param->result);
    free(param);
    return 0;
}