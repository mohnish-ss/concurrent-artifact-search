#include "search_internal.h"

#include <errno.h>
#include <sched.h>
#include <stdlib.h>

enum {
    SEARCH_OK = 0,
    SEARCH_INVALID_ARGUMENT = 1,
    SEARCH_ALLOCATION_FAILURE = 2,
    SEARCH_SYNCHRONIZATION_FAILURE = 3,
    SEARCH_THREAD_FAILURE = 4,
    SEARCH_VERIFICATION_FAILURE = 5
};

static void free_item_chain(SearchItem *item)
{
    while (item != NULL) {
        SearchItem *next = item->next;
        free(item);
        item = next;
    }
}

static void destroy_simulation(SearchSimulation *simulation, size_t initialized_sites,
                               bool result_lock_initialized)
{
    if (simulation == NULL) {
        return;
    }

    if (simulation->sites != NULL) {
        for (size_t index = 0; index < simulation->site_count; ++index) {
            free_item_chain(simulation->sites[index].items);
        }
        for (size_t index = 0; index < initialized_sites; ++index) {
            (void)search_lock_destroy(&simulation->sites[index].lock);
        }
    }

    free_item_chain(simulation->results.head);
    if (result_lock_initialized) {
        (void)search_lock_destroy(&simulation->results.lock);
    }

    free(simulation->threads);
    free(simulation->workers);
    free(simulation->sites);
}

static int initialize_simulation(SearchSimulation *simulation, const SearchConfig *config,
                                 size_t *initialized_sites, bool *result_lock_initialized)
{
    simulation->site_count = config->site_count;
    simulation->worker_count = config->worker_count;
    simulation->sites = calloc(config->site_count, sizeof(*simulation->sites));
    simulation->workers = calloc(config->worker_count, sizeof(*simulation->workers));
    simulation->threads = calloc(config->worker_count, sizeof(*simulation->threads));

    if (simulation->sites == NULL || simulation->workers == NULL ||
        simulation->threads == NULL) {
        return SEARCH_ALLOCATION_FAILURE;
    }

    if (search_lock_init(&simulation->results.lock) != 0) {
        return SEARCH_SYNCHRONIZATION_FAILURE;
    }
    *result_lock_initialized = true;

    for (size_t index = 0; index < config->site_count; ++index) {
        simulation->sites[index].index = index;
        if (search_lock_init(&simulation->sites[index].lock) != 0) {
            return SEARCH_SYNCHRONIZATION_FAILURE;
        }
        ++(*initialized_sites);
    }

    for (size_t item_index = 0; item_index < config->item_count; ++item_index) {
        SearchItem *item = malloc(sizeof(*item));
        if (item == NULL) {
            return SEARCH_ALLOCATION_FAILURE;
        }

        item->id = (uint64_t)item_index + UINT64_C(1);
        item->score = 10U + (unsigned)((item_index * 17U) % 91U);

        size_t site_index = (item_index * 7U + 3U) % config->site_count;
        item->next = simulation->sites[site_index].items;
        simulation->sites[site_index].items = item;
    }

    for (size_t worker_index = 0; worker_index < config->worker_count; ++worker_index) {
        SearchWorker *worker = &simulation->workers[worker_index];
        worker->id = worker_index;
        worker->site_index = worker_index % config->site_count;
        worker->direction = worker_index % 2U == 0U ? 1 : -1;
        worker->simulation = simulation;
        ++simulation->sites[worker->site_index].occupants;
    }

    atomic_init(&simulation->remaining_items, config->item_count);
    atomic_init(&simulation->stop_requested, false);
    atomic_init(&simulation->worker_failed, false);
    return SEARCH_OK;
}

static size_t adjacent_site(const SearchWorker *worker)
{
    const size_t count = worker->simulation->site_count;
    if (worker->direction > 0) {
        return (worker->site_index + 1U) % count;
    }
    return (worker->site_index + count - 1U) % count;
}

static int move_worker(SearchWorker *worker, size_t destination_index)
{
    SearchSimulation *simulation = worker->simulation;
    SearchSite *origin = &simulation->sites[worker->site_index];
    SearchSite *destination = &simulation->sites[destination_index];
    SearchSite *first = origin->index < destination->index ? origin : destination;
    SearchSite *second = origin->index < destination->index ? destination : origin;

    if (search_lock_acquire(&first->lock) != 0) {
        return SEARCH_SYNCHRONIZATION_FAILURE;
    }
    if (search_lock_acquire(&second->lock) != 0) {
        (void)search_lock_release(&first->lock);
        return SEARCH_SYNCHRONIZATION_FAILURE;
    }

    if (origin->occupants == 0U) {
        (void)search_lock_release(&second->lock);
        (void)search_lock_release(&first->lock);
        return SEARCH_VERIFICATION_FAILURE;
    }

    --origin->occupants;
    ++destination->occupants;
    worker->site_index = destination_index;
    ++worker->moves;

    if (search_lock_release(&second->lock) != 0 ||
        search_lock_release(&first->lock) != 0) {
        return SEARCH_SYNCHRONIZATION_FAILURE;
    }
    return SEARCH_OK;
}

static int take_item_from_current_site(SearchWorker *worker, SearchItem **item)
{
    SearchSite *site = &worker->simulation->sites[worker->site_index];

    if (search_lock_acquire(&site->lock) != 0) {
        return SEARCH_SYNCHRONIZATION_FAILURE;
    }

    *item = site->items;
    if (*item != NULL) {
        site->items = (*item)->next;
        (*item)->next = NULL;
    }

    if (search_lock_release(&site->lock) != 0) {
        return SEARCH_SYNCHRONIZATION_FAILURE;
    }
    return SEARCH_OK;
}

static int publish_item(SearchWorker *worker, SearchItem *item)
{
    SearchSimulation *simulation = worker->simulation;

    if (search_lock_acquire(&simulation->results.lock) != 0) {
        return SEARCH_SYNCHRONIZATION_FAILURE;
    }

    item->next = simulation->results.head;
    simulation->results.head = item;
    ++simulation->results.count;
    simulation->results.total_score += item->score;

    if (search_lock_release(&simulation->results.lock) != 0) {
        return SEARCH_SYNCHRONIZATION_FAILURE;
    }

    ++worker->collected_items;
    worker->collected_score += item->score;
    (void)atomic_fetch_sub_explicit(&simulation->remaining_items, 1U, memory_order_release);
    return SEARCH_OK;
}

static void mark_worker_failure(SearchSimulation *simulation)
{
    atomic_store_explicit(&simulation->worker_failed, true, memory_order_release);
    atomic_store_explicit(&simulation->stop_requested, true, memory_order_release);
}

static void *worker_main(void *argument)
{
    SearchWorker *worker = argument;
    SearchSimulation *simulation = worker->simulation;

    while (!atomic_load_explicit(&simulation->stop_requested, memory_order_acquire) &&
           atomic_load_explicit(&simulation->remaining_items, memory_order_acquire) > 0U) {
        if (move_worker(worker, adjacent_site(worker)) != SEARCH_OK) {
            mark_worker_failure(simulation);
            break;
        }

        SearchItem *item = NULL;
        if (take_item_from_current_site(worker, &item) != SEARCH_OK) {
            mark_worker_failure(simulation);
            break;
        }

        if (item != NULL) {
            if (publish_item(worker, item) != SEARCH_OK) {
                free(item);
                mark_worker_failure(simulation);
                break;
            }
        } else {
            sched_yield();
        }
    }

    return NULL;
}

static bool verify_simulation(const SearchSimulation *simulation, const SearchConfig *config,
                              SearchSummary *summary)
{
    size_t site_occupants = 0U;
    size_t worker_items = 0U;
    size_t worker_moves = 0U;
    unsigned long worker_score = 0U;
    size_t result_items = 0U;
    unsigned long result_score = 0U;

    for (size_t index = 0; index < simulation->site_count; ++index) {
        if (simulation->sites[index].items != NULL) {
            return false;
        }
        site_occupants += simulation->sites[index].occupants;
    }

    for (size_t index = 0; index < simulation->worker_count; ++index) {
        const SearchWorker *worker = &simulation->workers[index];
        if (worker->site_index >= simulation->site_count) {
            return false;
        }
        worker_items += worker->collected_items;
        worker_score += worker->collected_score;
        worker_moves += worker->moves;
    }

    for (const SearchItem *item = simulation->results.head; item != NULL; item = item->next) {
        ++result_items;
        result_score += item->score;
    }

    summary->collected_items = simulation->results.count;
    summary->total_score = simulation->results.total_score;
    summary->total_moves = worker_moves;

    return !atomic_load_explicit(&simulation->worker_failed, memory_order_acquire) &&
           atomic_load_explicit(&simulation->remaining_items, memory_order_acquire) == 0U &&
           site_occupants == config->worker_count &&
           worker_items == config->item_count &&
           worker_score == simulation->results.total_score &&
           result_items == simulation->results.count &&
           result_score == simulation->results.total_score &&
           simulation->results.count == config->item_count;
}

int search_simulation_run(const SearchConfig *config, SearchSummary *summary)
{
    if (config == NULL || summary == NULL || config->worker_count == 0U ||
        config->site_count < 2U || config->item_count == 0U) {
        return SEARCH_INVALID_ARGUMENT;
    }

    *summary = (SearchSummary){0};
    SearchSimulation simulation = {0};
    size_t initialized_sites = 0U;
    size_t created_threads = 0U;
    bool result_lock_initialized = false;

    int status = initialize_simulation(&simulation, config, &initialized_sites,
                                       &result_lock_initialized);
    if (status != SEARCH_OK) {
        destroy_simulation(&simulation, initialized_sites, result_lock_initialized);
        return status;
    }

    for (; created_threads < config->worker_count; ++created_threads) {
        if (pthread_create(&simulation.threads[created_threads], NULL, worker_main,
                           &simulation.workers[created_threads]) != 0) {
            atomic_store_explicit(&simulation.stop_requested, true, memory_order_release);
            status = SEARCH_THREAD_FAILURE;
            break;
        }
    }

    for (size_t index = 0; index < created_threads; ++index) {
        if (pthread_join(simulation.threads[index], NULL) != 0) {
            status = SEARCH_THREAD_FAILURE;
        }
    }

    if (status == SEARCH_OK) {
        summary->verified = verify_simulation(&simulation, config, summary);
        if (!summary->verified) {
            status = SEARCH_VERIFICATION_FAILURE;
        }
    }

    destroy_simulation(&simulation, initialized_sites, result_lock_initialized);
    return status;
}
