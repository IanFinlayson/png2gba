# Makefile

CC=gcc
FLAGS=-g -W -Wall
TARGET=png2gba
 
# do everything
all: tags $(TARGET)
	@echo "All done!"

# link it all together
$(TARGET): png2gba.c
	$(CC) $(FLAGS) -o $(TARGET) png2gba.c -lpng
 
# tidy up
clean:
	rm -f $(TARGET) tags

# create ctags index
tags:
	ctags png2gba.c

