EXE=hipri
CFLAGS += -Werror -pthread

$(EXE): $(EXE).c
	$(CC) $(CFLAGS) $(EXE).c -o $(EXE)

clean:
	rm -rf $(EXE) *.o *~
