
all: dlcall

dlcall: dlcall.c
	$(CC) -ldl -lm -o dlcall dlcall.c
