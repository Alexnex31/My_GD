##
## ALEXNEX PROJECT, 2026
## Makefile
## File description:
## Makefile for my_gd
##

CC        = gcc
CFLAGS    = -Wall -Wextra -Iinclude -MMD -MP -ffp-contract=off
LDLIBS    = -lcsfml-graphics -lcsfml-window -lcsfml-system -lcsfml-audio -lX11 -lm

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
UI_SRC    = $(wildcard src/ui/*.c)
GAME_SRC  = $(wildcard src/*.c)
SIM_OBJ   = $(SIM_SRC:%.c=$(OUT)/%.o)
UI_OBJ    = $(UI_SRC:%.c=$(OUT)/%.o)
GAME_OBJ  = $(GAME_SRC:%.c=$(OUT)/%.o)
DEP       = $(SIM_OBJ:.o=.d) $(UI_OBJ:.o=.d) $(GAME_OBJ:.o=.d)
TEST_SRC  = $(wildcard tests/*.c)
HDR       = $(wildcard include/sim/*.h) $(wildcard include/ui/*.h) $(wildcard tests/*.h)
TEST_FLAGS = -Wall -Wextra -Iinclude -ffp-contract=off -g -fsanitize=address,undefined

all: $(NAME)

$(NAME): $(GAME_OBJ) $(UI_OBJ) $(SIM_OBJ)
	$(CC) $^ -o $@ $(LDFLAGS) $(LDLIBS)

$(OUT)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

debug:
	$(MAKE) BUILD=debug

# Tests link ONLY the simulation and the toolkit's core: no CSFML, no window.
unit_tests: $(TEST_SRC) $(SIM_SRC) $(UI_SRC) $(HDR)
	$(CC) $(TEST_FLAGS) $(filter %.c,$^) -o $@ -lm

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
