.PHONY : clean all
.DEFAULT_GOAL := all

UNAME := $(shell uname -m -s)
OS = $(word 1, $(UNAME))

CC=gcc
FLAGS=-g -W -Wall
LINK_FLAGS=-lpng
TARGET=png2gba

ifeq ($(OS),Darwin)
	LINK_FLAGS += -largp
endif

# do everything (default, for linux)
all: $(TARGET)
	@echo "All done!"

# link it all together
$(TARGET): png2gba.c
	$(CC) $(FLAGS) -o $(TARGET) png2gba.c $(LINK_FLAGS)

# tidy up
clean:
	rm -f $(TARGET)

