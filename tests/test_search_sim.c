#include "search_sim.h"

#include <assert.h>
#include <stdio.h>

static void run_case(size_t workers, size_t sites, size_t items)
{
    const SearchConfig config = {
        .worker_count = workers,
        .site_count = sites,
        .item_count = items,
    };
    SearchSummary summary;

    assert(search_simulation_run(&config, &summary) == 0);
    assert(summary.verified);
    assert(summary.collected_items == items);
    assert(summary.total_score > 0U);
    assert(summary.total_moves >= items);
}

int main(void)
{
    run_case(1U, 2U, 1U);
    run_case(4U, 5U, 40U);
    run_case(12U, 7U, 250U);

    SearchSummary summary;
    const SearchConfig invalid = {0};
    assert(search_simulation_run(&invalid, &summary) != 0);

    puts("Concurrent search tests passed.");
    return 0;
}
