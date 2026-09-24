##
## ALEXNEX PROJECT, 2026
## Makefile
## File description:
## Makefile for my_gd
##

CC        = gcc
CFLAGS    = -Wall -Wextra -Iinclude -MMD -MP -ffp-contract=off
LDLIBS    = -lcsfml-graphics -lcsfml-window -lcsfml-system -lcsfml-audio -lm

BUILD     ?= release
ifeq ($(BUILD),debug)
    CFLAGS  += -g3 -O0 -fsanitize=address,undefined
    LDFLAGS += -fsanitize=address,undefined
    NAME    = my_gd_debug
else
    CFLAGS  += -O2
    NAME    = my_gd
endif
OUT       = build/$(BUILD)

SIM_SRC   = $(wildcard src/sim/*.c)
GAME_SRC  = $(wildcard src/*.c)
SIM_OBJ   = $(SIM_SRC:%.c=$(OUT)/%.o)
GAME_OBJ  = $(GAME_SRC:%.c=$(OUT)/%.o)
DEP       = $(SIM_OBJ:.o=.d) $(GAME_OBJ:.o=.d)
TEST_SRC  = $(wildcard tests/*.c)
TEST_FLAGS = -Wall -Wextra -Iinclude -ffp-contract=off -g -fsanitize=address,undefined

all: $(NAME)

$(NAME): $(GAME_OBJ) $(SIM_OBJ)
	$(CC) $^ -o $@ $(LDFLAGS) $(LDLIBS)

$(OUT)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

debug:
	$(MAKE) BUILD=debug

# Tests link ONLY the simulation: no CSFML, no window, no audio.
unit_tests: $(TEST_SRC) $(SIM_SRC)
	$(CC) $(TEST_FLAGS) $^ -o $@ -lm

test: unit_tests
	./unit_tests

fuzz_parser: tests/fuzz/fuzz_parser.c $(SIM_SRC)
	clang -Iinclude -ffp-contract=off -g -fsanitize=fuzzer,address,undefined $^ -o $@ -lm

clean:
	rm -rf build
	find . -type f \( -name '*~' -or -name '#*#' \) -delete

fclean: clean
	rm -f my_gd my_gd_debug unit_tests fuzz_parser

re: fclean
	$(MAKE) all

-include $(DEP)

.PHONY: all debug test clean fclean re
