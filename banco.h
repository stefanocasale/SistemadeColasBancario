#ifndef BANCO_H
#define BANCO_H

#include <stdint.h> 

typedef struct cliente {
    int id;
    double llegada;      // Tiempo de llegada A_i
} cliente_t;

void inicializar_cola(int capacidad);       // Inicializa la cola circular con la capacidad dada
void agregar_cliente(cliente_t *cliente);   // Agrega un cliente a la cola circular, bloqueando si la cola está llena
cliente_t* obtener_cliente();              // Obtiene el siguiente cliente de la cola circular
void liberar_cola();                       // Libera los recursos asociados a la cola circular

#endif