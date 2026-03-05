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

typedef struct estadisticas
{
    double total_wq;       // Suma de los tiempos de espera en la cola
    double total_w;        // Suma de los tiempos de espera total
    double max_espera;     // Tiempo de espera máximo registrado
    double ultimo_fin;     // Tiempo de finalización del último cliente atendido
    int atendidos;         // Número total de clientes atendidos
    pthread_mutex_t mutex; // Mutex para sincronización
} estadisticas_t;

static estadisticas_t estadisticas_clientes;

/*
 * @brief Inicializa la cola circular con la capacidad dada
 *
 * @param capacidad_maxima La capacidad máxima de la cola circular
 */
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

/*
 * @brief Agrega un cliente a la cola circular, bloqueando si la cola está llena
 *
 * @param cli El cliente a agregar a la cola circular
 */
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

/**
 * @brief Obtiene el siguiente cliente de la cola circular
 *
 * @return Puntero al cliente obtenido o NULL si la cola está vacía
 */
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

/**
 * @brief Libera los recursos asociados a la cola circular
 */
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

/*
 * @brief Inicializa las estadísticas de los clientes
 */
void inicializar_estadisticas()
{
    // Inicializamos las estadísticas
    estadisticas_clientes.total_wq = 0.0;
    estadisticas_clientes.total_w = 0.0;
    estadisticas_clientes.max_espera = 0.0;
    estadisticas_clientes.ultimo_fin = 0.0;
    estadisticas_clientes.atendidos = 0;
    pthread_mutex_init(&estadisticas_clientes.mutex, NULL);
}

/*
 * @brief Agrega una estadística de un cliente atendido
 *
 * @param wq El tiempo de espera en la cola del cliente
 * @param w El tiempo de espera total del cliente
 * @param fin El tiempo de finalización del cliente
 */
void agregar_estadistica(double wq, double w, double fin)
{
    // Bloqueamos el mutex
    pthread_mutex_lock(&estadisticas_clientes.mutex);

    // Actualizamos las estadísticas
    estadisticas_clientes.total_wq += wq;
    estadisticas_clientes.total_w += w;
    if (wq > estadisticas_clientes.max_espera)
    {
        estadisticas_clientes.max_espera = wq;
    }
    if (fin > estadisticas_clientes.ultimo_fin)
    {
        estadisticas_clientes.ultimo_fin = fin;
    }
    estadisticas_clientes.atendidos++;

    // Liberamos el mutex
    pthread_mutex_unlock(&estadisticas_clientes.mutex);
}

/*
 * @brief Devuelve el promedio de los tiempos de espera en la cola
 *
 * @return El promedio de los tiempos de espera en la cola
 */
double estadistica_promedio_wq()
{
    // Bloqueamos el mutex
    pthread_mutex_lock(&estadisticas_clientes.mutex);

    // inicializamos el promedio en 0.0
    double promedio = 0.0;

    // Calculamos el promedio solo si se han atendido clientes
    if (estadisticas_clientes.atendidos > 0)
    {
        promedio = estadisticas_clientes.total_wq / estadisticas_clientes.atendidos;
    }

    // Liberamos el mutex
    pthread_mutex_unlock(&estadisticas_clientes.mutex);

    return promedio;
}

/*
 * @brief Devuelve el promedio de los tiempos de espera total
 *
 * @return El promedio de los tiempos de espera total
 */
double estadistica_promedio_w()
{
    // Bloqueamos el mutex
    pthread_mutex_lock(&estadisticas_clientes.mutex);

    // inicializamos el promedio en 0.0
    double promedio = 0.0;

    // Calculamos el promedio solo si se han atendido clientes
    if (estadisticas_clientes.atendidos > 0)
    {
        promedio = estadisticas_clientes.total_w / estadisticas_clientes.atendidos;
    }

    // Liberamos el mutex
    pthread_mutex_unlock(&estadisticas_clientes.mutex);

    return promedio;
}

/*
 * @brief Devuelve el tiempo de espera máximo registrado
 *
 * @return El tiempo de espera máximo registrado
 */
double estadistica_max_espera()
{
    // Bloqueamos el mutex
    pthread_mutex_lock(&estadisticas_clientes.mutex);

    // Obtenemos el tiempo de espera máximo registrado
    double max_espera = estadisticas_clientes.max_espera;

    // Liberamos el mutex
    pthread_mutex_unlock(&estadisticas_clientes.mutex);

    return max_espera;
}

/*
 * @brief Devuelve el tiempo de finalización del último cliente atendido
 *
 * @return El tiempo de finalización del último cliente atendido
 */
double estadistica_ultimo_fin()
{
    // Bloqueamos el mutex
    pthread_mutex_lock(&estadisticas_clientes.mutex);

    // Obtenemos el tiempo de finalización del último cliente atendido
    double ultimo_fin = estadisticas_clientes.ultimo_fin;

    // Liberamos el mutex
    pthread_mutex_unlock(&estadisticas_clientes.mutex);

    return ultimo_fin;
}

/*
 * @brief Devuelve el número total de clientes atendidos
 *
 * @return El número total de clientes atendidos
 */
int estadistica_atendidos()
{
    // Bloqueamos el mutex
    pthread_mutex_lock(&estadisticas_clientes.mutex);

    // Obtenemos el número total de clientes atendidos
    int atendidos = estadisticas_clientes.atendidos;

    // Liberamos el mutex
    pthread_mutex_unlock(&estadisticas_clientes.mutex);

    return atendidos;
}

/*
 * @brief Libera los recursos asociados a las estadísticas
 */
void estadistica_destroy()
{
    pthread_mutex_destroy(&estadisticas_clientes.mutex);
}