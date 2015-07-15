SRCS=\
	main.c \
  $(wildcard spiffs/src/*.c) \

CFLAGS=-Wall -Wextra -Wno-unused-parameter -Wno-unused-function -std=c99 -I. -Ispiffs/src

spiffsimg: $(SRCS)
	$(CC) $(CFLAGS) $^ $(LDFLAGS) -o $@
