CC ?= cc
CPPFLAGS := -Iinclude -Isrc
CFLAGS := -std=c11 -Wall -Wextra -Wpedantic -Werror -O2
THREAD_FLAGS := -pthread

APP := artifact-search
TEST_APP := test-search
LIB_SOURCES := src/search_sim.c src/sync_lock.c

.PHONY: all clean test asan tsan

all: $(APP)

$(APP): src/main.c $(LIB_SOURCES) include/search_sim.h src/search_internal.h
	$(CC) $(CPPFLAGS) $(CFLAGS) $(THREAD_FLAGS) -o $@ src/main.c $(LIB_SOURCES)

$(TEST_APP): tests/test_search_sim.c $(LIB_SOURCES) include/search_sim.h src/search_internal.h
	$(CC) $(CPPFLAGS) $(CFLAGS) $(THREAD_FLAGS) -o $@ tests/test_search_sim.c $(LIB_SOURCES)

test: $(TEST_APP)
	./$(TEST_APP)

asan:
	$(CC) $(CPPFLAGS) -std=c11 -Wall -Wextra -Wpedantic -Werror -O1 -g \
		-fsanitize=address,undefined $(THREAD_FLAGS) -o $(TEST_APP)-asan \
		tests/test_search_sim.c $(LIB_SOURCES)
	./$(TEST_APP)-asan

tsan:
	$(CC) $(CPPFLAGS) -std=c11 -Wall -Wextra -Wpedantic -Werror -O1 -g \
		-fsanitize=thread $(THREAD_FLAGS) -o $(TEST_APP)-tsan \
		tests/test_search_sim.c $(LIB_SOURCES)
	./$(TEST_APP)-tsan

clean:
	rm -f $(APP) $(TEST_APP) $(TEST_APP)-asan $(TEST_APP)-tsan *.o src/*.o tests/*.o
	rm -rf $(APP).dSYM $(TEST_APP).dSYM $(TEST_APP)-asan.dSYM $(TEST_APP)-tsan.dSYM
