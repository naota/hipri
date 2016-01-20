EXE=hipri
CFLAGS += -std=gnu99 -Werror -pthread

$(EXE): $(EXE).c
	$(CC) $(CFLAGS) $(EXE).c -o $(EXE)

clean:
	rm -rf $(EXE) *.o *~
