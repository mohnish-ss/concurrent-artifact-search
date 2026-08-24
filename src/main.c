#include "search_sim.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

enum {
    DEFAULT_WORKERS = 8,
    DEFAULT_SITES = 6,
    DEFAULT_ITEMS = 60,
    MAX_INPUT_VALUE = 100000
};

static int parse_count(const char *text, size_t *value)
{
    char *end = NULL;
    errno = 0;
    unsigned long parsed = strtoul(text, &end, 10);

    if (errno != 0 || end == text || *end != '\0' || parsed == 0U ||
        parsed > MAX_INPUT_VALUE) {
        return -1;
    }

    *value = (size_t)parsed;
    return 0;
}

int main(int argc, char **argv)
{
    SearchConfig config = {
        .worker_count = DEFAULT_WORKERS,
        .site_count = DEFAULT_SITES,
        .item_count = DEFAULT_ITEMS,
    };

    if (argc != 1 && argc != 4) {
        fprintf(stderr, "Usage: %s [workers sites items]\n", argv[0]);
        return EXIT_FAILURE;
    }

    if (argc == 4 &&
        (parse_count(argv[1], &config.worker_count) != 0 ||
         parse_count(argv[2], &config.site_count) != 0 ||
         parse_count(argv[3], &config.item_count) != 0 || config.site_count < 2U)) {
        fprintf(stderr, "Counts must be positive integers; sites must be at least 2.\n");
        return EXIT_FAILURE;
    }

    SearchSummary summary;
    int status = search_simulation_run(&config, &summary);
    if (status != 0) {
        fprintf(stderr, "Simulation failed with status %d.\n", status);
        return EXIT_FAILURE;
    }

    printf("Workers: %zu\n", config.worker_count);
    printf("Search sites: %zu\n", config.site_count);
    printf("Items collected: %zu\n", summary.collected_items);
    printf("Combined item score: %lu\n", summary.total_score);
    printf("Worker moves: %zu\n", summary.total_moves);
    printf("Invariant verification: %s\n", summary.verified ? "passed" : "failed");
    return summary.verified ? EXIT_SUCCESS : EXIT_FAILURE;
}
