# Definiamo il compilatore
CC = gcc

# Definiamo i percorsi di Homebrew
CFLAGS = -I/opt/homebrew/include
LDFLAGS = -L/opt/homebrew/lib -lSDL2

# Il nome del nostro programma finale
TARGET = sbl

# La regola per costruire il programma
all:
	$(CC) sferaBL.c -o $(TARGET) $(CFLAGS) $(LDFLAGS)

# Regola per pulire i file compilati
clean:
	rm -f $(TARGET)
