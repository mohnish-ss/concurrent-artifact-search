#ifndef SEARCH_SIM_H
#define SEARCH_SIM_H

#include <stdbool.h>
#include <stddef.h>

typedef struct {
    size_t worker_count;
    size_t site_count;
    size_t item_count;
} SearchConfig;

typedef struct {
    size_t collected_items;
    unsigned long total_score;
    size_t total_moves;
    bool verified;
} SearchSummary;

int search_simulation_run(const SearchConfig *config, SearchSummary *summary);

#endif
