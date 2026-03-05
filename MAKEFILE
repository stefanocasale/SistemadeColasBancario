# Compilador y flags
CC = gcc
CFLAGS = -std=c11 -Wall -Wextra -Werror -pthread
LDFLAGS = -lm

# Nombre del ejecutable y archivo fuente
TARGET = banco
SRC = banco.c

# Regla por defecto
all: $(TARGET)

# Compilación del programa
$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

# Limpieza de archivos generados
clean:
	rm -f $(TARGET)