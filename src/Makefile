CC=gcc

CFLAGS=-Wall -Wextra -Wshadow -pedantic -g -MMD -fPIC
MODE=debug
ifeq ($(MODE),debug)
	CFLAGS+=-O0
else ifeq ($(MODE),release)
	CFLAGS+=-O2
else ifeq ($(MODE),prof)
	CFLAGS+=-pg -O2
endif

INCLUDES=-I./
RM=rm -rf

BUILDPATH=.build
SRCS=$(wildcard *.c)
OBJS=$(addprefix $(BUILDPATH)/, $(notdir $(SRCS:.c=.o)))
DEPS=$(OBJS:.o=.d)

ifeq ($(OS), Windows_NT)
	INCLUDES+=-I./nt/
else
	UNAME_S := $(shell uname -s)
	ifeq ($(UNAME_S), Linux)
		INCLUDES+=-I./unix/
		INCLUDES+=-I./linux/
	endif
	ifeq ($(UNAME_S), FreeBSD)
		INCLUDES+=-I./unix/
		INCLUDES+=-I./bsd/
	endif
endif

default: $(OBJS)

$(BUILDPATH)/%.o: %.c
	@-mkdir -p $(BUILDPATH)
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ -c $<

all: clean $(OBJS)

clean:
	@$(RM) $(BUILDPATH)/*.o $(BUILDPATH)/*.d
	@$(RM) $(BUILDPATH)

-include $(DEPS)
