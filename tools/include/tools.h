//
// Created by huska on 2025/5/15.
//

#include <taos.h>
#include <semaphore.h>

#ifndef STABILITYTESTING_TOOLS_H
#define STABILITYTESTING_TOOLS_H
int execute_query(TAOS *taos, const char *query, bool fetch_result, int64_t reqid);
int execute_async(TAOS *taos, const char *query, bool fetch_result, sem_t *sem, int64_t reqid);
#endif //STABILITYTESTING_TOOLS_H