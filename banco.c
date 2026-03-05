#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include "banco.h"

// Arreglo circular para clientes
typedef struct cola_circular
{
    cliente_t **clientes;  // Arreglo de punteros a clientes
    int capacidad;         // Capacidad máxima de la cola
    int head;              // Índice del primer cliente
    int tail;              // Índice del último cliente
    int count;             // Número de clientes en la cola
    pthread_mutex_t mutex; // Mutex para sincronización
} cola_circular_t;

static cola_circular_t cola_clientes;

void inicializar_cola(int capacidad_maxima)
{
    // Asignar memoria para el arreglo de punteros a clientes
    cola_clientes.clientes = (cliente_t **)malloc(capacidad_maxima * sizeof(cliente_t *));

    // Verificar si la asignación de memoria fue exitosa
    if (cola_clientes.clientes == NULL)
    {
        exit(EXIT_FAILURE);
    }

    // Inicializar los índices y el contador
    cola_clientes.capacidad = capacidad_maxima;
    cola_clientes.head = 0;
    cola_clientes.tail = 0;
    cola_clientes.count = 0;
    pthread_mutex_init(&cola_clientes.mutex, NULL);
}

// Agrega un cliente a la cola circular, bloqueando si la cola está llena
void agregar_cliente(cliente_t *cli)
{
    // Bloqueamos el mutex
    pthread_mutex_lock(&cola_clientes.mutex);

    // Verificamos si hay espacio en la cola
    if (cola_clientes.count < cola_clientes.capacidad)
    {
        // Agremaos un cliente
        cola_clientes.clientes[cola_clientes.tail] = cli;

        // Manejamos el índice circular
        cola_clientes.tail = (cola_clientes.tail + 1) % cola_clientes.capacidad;

        cola_clientes.count++;
    }
    else
    {
        // Cola llena
        fprintf(stderr, "Error: Cliente %d no puede ser agregado, cola llena.\n", cli->id);

        // Liberamos el cliente
        free(cli);
    }

    // Liberar el mutex
    pthread_mutex_unlock(&cola_clientes.mutex);
}

// Obtiene el siguiente cliente de la cola circular
cliente_t *obtener_cliente()
{
    cliente_t *cli = NULL;

    // Bloqueamos el mutex
    pthread_mutex_lock(&cola_clientes.mutex);

    // Verificamos si hay clientes en la cola
    if (cola_clientes.count > 0)
    {
        // Obtenemos el cliente del frente de la cola
        cli = cola_clientes.clientes[cola_clientes.head];

        // Manejamos el índice circular
        cola_clientes.head = (cola_clientes.head + 1) % cola_clientes.capacidad;

        cola_clientes.count--;
    }

    // Liberamos el mutex
    pthread_mutex_unlock(&cola_clientes.mutex);

    return cli;
}

// Libera los recursos asociados a la cola circular
void liberar_cola()
{

    // Liberamos la memoria de los clientes restantes en la cola
    for (int i = 0; i < cola_clientes.count; i++)
    {
        // Calculamos el índice del cliente a liberar
        int index = (cola_clientes.head + i) % cola_clientes.capacidad;

        // Liberamos el cliente
        free(cola_clientes.clientes[index]);
    }

    // Liberamos el arreglo de punteros a clientes
    free(cola_clientes.clientes);

    // Destruimos el mutex
    pthread_mutex_destroy(&cola_clientes.mutex);
}