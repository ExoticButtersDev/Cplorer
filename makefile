
CC := gcc
CFLAGS := -Wall -Wextra -O2

ifeq ($(OS),Windows_NT)
    TARGET := Cplorer.exe
    LDFLAGS := -mwindows -municode -lcomctl32 -lshlwapi
    RC := windres
    ICON_OBJ := Cplorer_res.o
else
    TARGET := Cplorer
    LDFLAGS := -lgtk-3
    RC := echo
    ICON_OBJ :=
endif

SRCS := Cplorer.c
OBJS := $(SRCS:.c=.o) $(ICON_OBJ)

$(TARGET): $(OBJS)
	$(CC) $(OBJS) -o $(TARGET) $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

Cplorer_res.o: Cplorer.rc icon.ico
	$(RC) Cplorer.rc -o Cplorer_res.o

clean:
	rm -f $(OBJS) $(TARGET)

.PHONY: clean
