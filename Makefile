.PHONY : clean all
.DEFAULT_GOAL := all

CC=gcc
FLAGS=-g -W -Wall
TARGET=png2gba
 
# do everything (default, for linux)
all: $(TARGET)
	@echo "All done!"

# link it all together
$(TARGET): png2gba.c
	$(CC) $(FLAGS) -o $(TARGET) png2gba.c -lpng

# tidy up
clean:
	rm -f $(TARGET)

