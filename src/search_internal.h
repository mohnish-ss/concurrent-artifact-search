#ifndef SEARCH_INTERNAL_H
#define SEARCH_INTERNAL_H

#include "search_sim.h"

#include <pthread.h>
#include <stdatomic.h>
#include <stdint.h>

#if !defined(__APPLE__)
#include <semaphore.h>
#endif

typedef struct {
#if defined(__APPLE__)
    pthread_mutex_t primitive;
#else
    sem_t primitive;
#endif
} SearchLock;

typedef struct SearchItem {
    uint64_t id;
    unsigned score;
    struct SearchItem *next;
} SearchItem;

typedef struct {
    size_t index;
    size_t occupants;
    SearchItem *items;
    SearchLock lock;
} SearchSite;

typedef struct {
    SearchItem *head;
    size_t count;
    unsigned long total_score;
    SearchLock lock;
} ResultSet;

struct SearchSimulation;

typedef struct {
    size_t id;
    size_t site_index;
    int direction;
    size_t collected_items;
    unsigned long collected_score;
    size_t moves;
    struct SearchSimulation *simulation;
} SearchWorker;

typedef struct SearchSimulation {
    SearchSite *sites;
    SearchWorker *workers;
    pthread_t *threads;
    size_t site_count;
    size_t worker_count;
    ResultSet results;
    atomic_size_t remaining_items;
    atomic_bool stop_requested;
    atomic_bool worker_failed;
} SearchSimulation;

int search_lock_init(SearchLock *lock);
int search_lock_acquire(SearchLock *lock);
int search_lock_release(SearchLock *lock);
int search_lock_destroy(SearchLock *lock);

#endif
