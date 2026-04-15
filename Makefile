##
## ALEXNEX PROJECT, 2026
## makefile
## File description:
## makefile for my gd project
##

SRC = src/gd.c \
	src/button.c \
	src/main_menu.c \
	src/option_menu.c \
	src/editor_menu.c \
	src/level_list_menu.c \
	src/keyboard_events.c \
	src/get_level.c \
	src/my_putchar.c \
	src/my_put_nbr.c \
	src/my_putstr.c \
	src/window.c \
	src/level.c	\
	src/player.c \
	src/physics.c \
	src/portal.c \
	src/utilitary.c \
	src/music.c \
	src/cursor.c \
	src/my_strcpy.c \
	src/my_str_to_wordarray.c

OBJ = $(SRC:.c=.o)

CC = epiclang

CFLAGS = -Iinclude

LDFLAGS = -lm -l csfml-graphics -l csfml-window -lcsfml-system -lcsfml-audio

NAME = my_gd

all: $(NAME)

$(NAME): $(OBJ)
	epiclang $(OBJ) -Iinclude -l csfml-graphics -l csfml-window -lcsfml-system -lcsfml-audio -lm -o $(NAME)

clean:
	rm -f $(OBJ)
	find -type f \( -name '*~' -or -name '#*#' \) -delete

fclean: clean
	rm -f $(NAME)

re: fclean all
