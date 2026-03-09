#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <math.h>
#include "banco.h"
#include <ctype.h>
#include <errno.h>
#include <time.h>
#include <stdint.h>
#include <float.h>
#include <string.h>

// Varias la cola circular y las estadísticas de los clientes
int banco_cerrado = 0;
double mu;

// Variables de sincronizaci'on
static pthread_mutex_t mutex_rand = PTHREAD_MUTEX_INITIALIZER;

// Variables para manejar eventos
static evento_t *eventos = NULL;
static int num_eventos = 0;
static int capacidad_eventos = 0;
static pthread_mutex_t mutex_eventos = PTHREAD_MUTEX_INITIALIZER;

// Arreglo circular para clientes
typedef struct cola_circular
{
    cliente_t **clientes;  // Arreglo de punteros a clientes
    int capacidad;         // Capacidad máxima de la cola
    int head;              // Índice del primer cliente
    int tail;              // Índice del último cliente
    int count;             // Número de clientes en la cola
    pthread_mutex_t mutex; // Mutex para sincronización
    pthread_cond_t cond;   // Condición para esperar a que haya clientes en la cola
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
 * @brief Extrae un cliente de la cola circular de manera segura
 *
 * @return Puntero al cliente extraído o NULL si la cola está vacía
 */
static cliente_t *extraer_cliente_locked()
{
    // Verificamos si la cola está vacía
    if (cola_clientes.count == 0)
    {
        return NULL;
    }

    // Extraemos el cliente del frente de la cola
    cliente_t *cli = cola_clientes.clientes[cola_clientes.head];
    cola_clientes.head = (cola_clientes.head + 1) % cola_clientes.capacidad;
    cola_clientes.count--;
    return cli;
}

/*
 * @brief Inicializa un arreglo dinámico de eventos
 */
void inicializar_eventos()
{
    capacidad_eventos = 100; // capacidad inicial
    eventos = malloc(capacidad_eventos * sizeof(evento_t));
    if (!eventos)
    {
        perror("Error al asignar memoria para eventos");
        exit(EXIT_FAILURE);
    }
    num_eventos = 0;
}

/*
 * @brief Agrega un nuevo evento al arreglo de eventos de forma segura para hilos.
 *
 * @param tiempo Tiempo en que ocurre el evento
 * @param tipo Tipo de evento
 * @param id_cliente ID del cliente asociado al evento
 * @param id_cajero ID del cajero asociado al evento
 */
void agregar_evento(double tiempo, tipo_evento_t tipo, int id_cliente, int id_cajero)
{
    // Bloqueamos el mutex
    pthread_mutex_lock(&mutex_eventos);

    // Si es necesario, expandir el array
    if (num_eventos >= capacidad_eventos)
    {
        capacidad_eventos *= 2;
        eventos = realloc(eventos, capacidad_eventos * sizeof(evento_t));
        if (!eventos)
        {
            perror("Error al reasignar memoria para eventos");
            exit(EXIT_FAILURE);
        }
    }

    // Añadir el nuevo evento
    eventos[num_eventos].tiempo = tiempo;
    eventos[num_eventos].tipo = tipo;
    eventos[num_eventos].id_cliente = id_cliente;
    eventos[num_eventos].id_cajero = id_cajero;
    num_eventos++;

    // Liberamos el Mutex
    pthread_mutex_unlock(&mutex_eventos);
}

/*
 * @brief Función de comparación para Quicksort
 *
 * @param a Puntero al primer evento
 * @param b Puntero al segundo evento
 */
static int comparar_eventos(const void *a, const void *b)
{
    const evento_t *ea = (const evento_t *)a;
    const evento_t *eb = (const evento_t *)b;

    // Comparamos
    if (ea->tiempo < eb->tiempo)
        return -1;
    if (ea->tiempo > eb->tiempo)
        return 1;
    return 0;
}

/*
 * @brief Imprime los eventos en orden cronológico
 */
void imprimir_eventos_ordenados()
{
    // Ordenar los eventos por tiempo
    qsort(eventos, num_eventos, sizeof(evento_t), comparar_eventos);

    // Imprimir en orden
    for (int i = 0; i < num_eventos; i++)
    {
        evento_t *e = &eventos[i];
        switch (e->tipo)
        {
        case EVENTO_LLEGADA:
            printf("[t=%.2f] Cliente %d llega al banco\n", e->tiempo, e->id_cliente);
            break;
        case EVENTO_INICIO:
            printf("[t=%.2f] Cliente %d inicia atencion en Cajero %d\n", e->tiempo, e->id_cliente, e->id_cajero);
            break;
        case EVENTO_FIN:
            printf("[t=%.2f] Cliente %d finaliza atencion en Cajero %d\n", e->tiempo, e->id_cliente, e->id_cajero);
            break;
        }
    }
}

/*
 *  @brief  Libera la memoria del arreglo de eventos y destruye su mutex.
 */
void liberar_eventos()
{
    // Liberamos eventos
    free(eventos);

    // Destruimos mutex
    pthread_mutex_destroy(&mutex_eventos);
}

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

    // Iniciamos los mutex
    pthread_mutex_init(&cola_clientes.mutex, NULL);
    pthread_cond_init(&cola_clientes.cond, NULL);
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

        // Hacemos signal a los cajeros que hay un nuevo cliente en la cola
        pthread_cond_signal(&cola_clientes.cond);
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

/*
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

    // Destruimos los mutex
    pthread_mutex_destroy(&cola_clientes.mutex);
    pthread_cond_destroy(&cola_clientes.cond);
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
 * @brief Genera un tiempo de servicio aleatorio
 */
double generar_servicio()
{
    // Bloqueamos el mutex para proteger el acceso a rand()
    pthread_mutex_lock(&mutex_rand);

    // Generamos un número aleatorio entre 0 y 1
    double U = (double)rand() / RAND_MAX;

    // Liberamos el mutex
    pthread_mutex_unlock(&mutex_rand);

    return -log(1 - U) / mu;
}

/*
 * @brief Función para que los cajeros atiendan a los clientes
 */
void *atender_clientes(void *arg)
{
    // Obtenemos el ID del cajero desde el argumento
    int id_cajero = (int)(intptr_t)arg;
    // printf("DEBUG: Cajero %d iniciando (tid=%lu)\n", id_cajero, pthread_self());
    double tiempo_libre = 0.0;

    // Creamos el ciclo de atencion de clientes para el cajero
    while (1)
    {
        // Obtener el siguiente cliente de la cola
        cliente_t *cliente = obtener_cliente();

        // Si no hay clientes en la cola salimos del ciclo
        if (cliente == NULL)
        {
            // printf("DEBUG: Cajero %d no obtuvo cliente y termina\n", id_cajero);
            break;
        }
        // printf("DEBUG: Cajero %d atendiendo cliente %d\n", id_cajero, cliente->id);

        // Si tenemos un cliente lo atendemos
        double B = (cliente->llegada > tiempo_libre) ? cliente->llegada : tiempo_libre;
        double S = generar_servicio();
        double F = B + S;

        // Agregamos los eventos de inicio y final
        agregar_evento(B, EVENTO_INICIO, cliente->id, id_cajero);
        agregar_evento(F, EVENTO_FIN, cliente->id, id_cajero);

        // Actualizamos las estadísticas del cliente atendido
        double wq = B - cliente->llegada; // Tiempo de espera en la cola
        double w = F - cliente->llegada;  // Tiempo de espera total
        agregar_estadistica(wq, w, F);

        // Actializamos el tiempo libre del cajero
        tiempo_libre = F;

        // Liberamos la memoria del cliente atendido
        free(cliente);

        // Balanceamos el trabajo de los Clientes
        sched_yield();
    }
    return NULL;
}

/*
 * @brief Obtiene el siguiente cliente de la cola circular
 */
cliente_t *obtener_cliente()
{
    // Bloqueamos el mutex
    pthread_mutex_lock(&cola_clientes.mutex);

    // Esperamos a que haya clientes en la cola o a que el banco se cierre
    while (cola_clientes.count == 0 && !banco_cerrado)
    {
        // Esperamos
        pthread_cond_wait(&cola_clientes.cond, &cola_clientes.mutex);
    }

    // Si el banco está cerrado y no hay clientes
    if (banco_cerrado && cola_clientes.count == 0)
    {
        // Liberamos el mutex
        pthread_mutex_unlock(&cola_clientes.mutex);

        return NULL;
    }

    // Extraemos un cliente de la cola
    cliente_t *cli = extraer_cliente_locked();

    // Liberamos el mutex
    pthread_mutex_unlock(&cola_clientes.mutex);

    return cli;
}

/*
 * @brief Imprime el resumen final de la simulación con todos los resultados.
 *
 * @param cajeros Número de cajeros  utilizado en la simulación.
 * @param tcierre Tiempo de cierre del banco.
 * @param lambda Tasa de llegada de clientes.
 * @param mu Tasa de servicio de los cajeros.
 * @param max_clientes Límite máximo de clientes .
 * @param clientes_atendidos Número total de clientes atendidos.
 * @param truncado Indica si se alcanzó el límite MAX_CLIENTES.
 * @param Wq_sim Tiempo promedio de espera en cola simulado.
 * @param W_sim Tiempo promedio en sistema (cola + servicio) simulado.
 * @param max_espera Tiempo máximo de espera en cola registrado.
 * @param tiempo_ultimo Tiempo de finalización del último cliente atendido.
 * @param rho Utilización del sistema (lambda / (c * mu)).
 * @param Wq_teo Tiempo promedio de espera en cola teórico (Erlang-C).
 * @param W_teo Tiempo promedio en sistema teórico.
 * @param estable Indica si el sistema es estable.
 */
void imprimir_resumen(int cajeros, int tcierre, double lambda, double mu,
                      int max_clientes, int clientes_atendidos, int truncado,
                      double Wq_sim, double W_sim, double max_espera, double tiempo_ultimo,
                      double rho, double Wq_teo, double W_teo, int estable)
{
    // Encabezado
    printf("=============================================\n");
    printf("                RESUMEN FINAL\n");
    printf("=============================================\n");

    // Parámetros
    printf("Parametros:\n");
    printf("    CAJEROS:        %d\n", cajeros);
    printf("    TCIERRE:        %d\n", tcierre);
    printf("    LAMBDA:         %.6g\n", lambda);
    printf("    MU:             %.6g\n", mu);
    printf("    MAX_CLIENTES:   %d\n", max_clientes);

    // Resultados Simulados
    printf("Resultados Simulados:\n");
    printf("    Clientes atendidos:                 %d\n", clientes_atendidos);
    printf("    Truncado por MAX_CLIENTES:          %s\n", truncado ? "SI" : "NO");
    printf("    Tiempo promedio de espera (Wq):     %.2f\n", Wq_sim);
    printf("    Tiempo promedio en sistema (W):     %.2f\n", W_sim);
    printf("    Tiempo maximo de espera:            %.2f\n", max_espera);
    printf("    Tiempo total hasta ultimo cliente:  %.2f\n", tiempo_ultimo);

    // Resultados Teóricos
    printf("Resultados Teoricos (M/M/c):\n");
    printf("    Utilizacion (rho):                  %.4f\n", rho);

    // Vemos si el sistema es estable
    if (!estable)
    {
        // Inestable
        printf("    Tiempo promedio de espera teorico: N/A\n");
        printf("    Tiempo promedio en sistema teorico: N/A\n");
        printf("    Error relativo Wq: N/A\n");
        printf("    Error relativo W: N/A\n");
    }
    else
    {
        // Estable
        printf("    Tiempo promedio de espera teorico:  %.2f\n", Wq_teo);
        printf("    Tiempo promedio en sistema teorico: %.2f\n", W_teo);

        // Calculos
        double err_Wq = 0.0;
        double err_W = 0.0;

        if (Wq_teo != 0.0)
            err_Wq = fabs((Wq_sim - Wq_teo) / Wq_teo) * 100.0;
        else
            err_Wq = INFINITY;
        if (W_teo != 0.0)
            err_W = fabs((W_sim - W_teo) / W_teo) * 100.0;
        else
            err_W = INFINITY;
        if (isfinite(err_Wq))
            printf("    Error relativo Wq:  %.1f %%\n", err_Wq);
        else
            printf("    Error relativo Wq: N/A\n");
        if (isfinite(err_W))
            printf("    Error relativo W:   %.1f %%\n", err_W);
        else
            printf("    Error relativo W: N/A\n");
    }

    // Estado del Sistema
    if (rho < 1.0)
    {
        printf("Estado del sistema:\n");
        printf("    rho = %.4f < 1 -> Sistema estable\n", rho);
    }
    else
    {
        printf("    Estado del sistema:\n");
        printf("    rho = %.4f >= 1 -> Sistema inestable\n", rho);
    }

    // Final del resumen
    printf("\n");
}

/*
 * @brief Calcula las metricas teoricas del modelo M/M/c usando la formula de Erlang‑C.
 *
 * @param cajeros Número de cajeros  utilizado en la simulación.
 * @param lambda Tasa de llegada de clientes.
 * @param mu Tasa de servicio de los cajeros
 * @param rho Utilización del sistema (lambda / (c * mu)).
 * @param Wq_teo Tiempo promedio de espera en cola teórico (Erlang-C).
 * @param W_teo Tiempo promedio en sistema teórico.
 * @param estable Indica si el sistema es estable.
 */
void calcular_teoricas(int cajeros, double lambda, double mu,
                       double *rho, double *Wq_teo, double *W_teo, int *estable)
{
    // Verificaciones
    if (cajeros <= 0 || lambda < 0.0 || mu <= 0.0 || rho == NULL || Wq_teo == NULL || W_teo == NULL || estable == NULL)
    {
        if (rho)
            *rho = NAN;
        if (Wq_teo)
            *Wq_teo = NAN;
        if (W_teo)
            *W_teo = NAN;
        if (estable)
            *estable = 0;
        fprintf(stderr, "calcular_teoricas: parametros invalidos.\n");
        return;
    }

    // Calculamos el Tráfico Ofrecido
    long double a = (long double)lambda / (long double)mu;

    // Calculamos la utilización del Sistema
    long double rho_ld = a / (long double)cajeros;
    *rho = (double)rho_ld;

    // Verificamos Inestabilidad
    if (rho_ld >= 1.0L)
    {
        // Notificamos Inestabilidad
        fprintf(stderr, "Warning: sistema inestable (rho = %.12g >= 1). Se omiten metricas teoricas.\n", (double)rho_ld);
        *Wq_teo = INFINITY;
        *W_teo = INFINITY;
        *estable = 0;
        return;
    }

    // Calculos necesarios
    long double suma = 0.0L;
    long double termino = 1.0L;
    suma += termino;

    // Acumulamos términos
    for (int k = 1; k <= cajeros - 1; ++k)
    {
        termino = termino * (a / (long double)k);
        suma += termino;
    }

    long double termino_c = termino * (a / (long double)cajeros); /* a^c / c! */

    // Factor Erlang-C
    long double numerador = termino_c / (1.0L - rho_ld);
    long double denominador = suma + numerador;

    long double Cc_a;
    if (denominador == 0.0L)
    {
        // Hacemos una protección numérica
        Cc_a = 0.0L;
    }
    else
    {
        Cc_a = numerador / denominador;
    }

    // Calculamos tiempos teóricos
    long double denom_Wq = (long double)cajeros * (long double)mu - (long double)lambda;

    // Verificamos
    if (denom_Wq <= 0.0L)
    {
        // Protegemos el denominador
        fprintf(stderr, "calcular_teoricas: denominador para Wq no positivo (posible inestabilidad numerica).\n");
        *Wq_teo = INFINITY;
        *W_teo = INFINITY;
        *estable = 0;
        return;
    }

    long double Wq_ld = Cc_a / denom_Wq;
    long double W_ld = Wq_ld + 1.0L / (long double)mu;

    *Wq_teo = (double)Wq_ld;
    *W_teo = (double)W_ld;
    *estable = 1;
}

/*
 * @brief Genera una muestra de una distribucion exponencial usando la transformacion
 * inversa aplicada a un numero uniforme generado por rand().
 *
 *  @param tasa Parametro de la exponencial (lambda), en unidades de eventos por
 *          unidad de tiempo. Debe ser > 0.
 *
 * @return double Muestra exponencial.
 */
static double exponencial_con_rand(double tasa)
{
    double u = (double)rand() / (RAND_MAX + 1.0); // U en [0,1)
    if (u <= 0.0)
        u = 1e-12;
    return -log(1.0 - u) / tasa;
}

/*
 * @param lambda  tasa de llegada (clientes/segundo)
 * @param tcierre Tiempo de cierre (segundos logicos)
 * @param max_clientes Limite superior para truncar
 * @param num_clientes Salida con la cantidad generada
 * @param truncado Salida 1 si se trunco por max_clientes, 0 si no
 */
cliente_t **generar_clientes(double lambda, int tcierre, int max_clientes,
                             int *num_clientes, int *truncado)
{
    // Validación inicial
    if (!num_clientes || !truncado || lambda <= 0.0 || tcierre <= 0 || max_clientes <= 0)
    {
        if (num_clientes)
            *num_clientes = 0;
        if (truncado)
            *truncado = 0;
        return NULL;
    }

    // Reservamos espacio al puntero
    cliente_t **vec = malloc(sizeof(cliente_t *) * (size_t)max_clientes);
    if (!vec)
    {
        *num_clientes = 0;
        *truncado = 0;
        return NULL;
    }

    // Definimos variables de contrl
    double acumulado = 0.0;
    int n = 0;
    int id = 0;

    while (n < max_clientes)
    {
        double ti = exponencial_con_rand(lambda);
        acumulado += ti;

        // Verificamos si registramos
        if (acumulado >= (double)tcierre)
            break;

        cliente_t *c = malloc(sizeof(cliente_t));
        if (!c)
        {
            // Si falla liberamos
            for (int j = 0; j < n; ++j)
                free(vec[j]);
            free(vec);
            *num_clientes = 0;
            *truncado = 0;
            return NULL;
        }

        // Asignamos ID
        id++;
        c->id = id;
        c->llegada = acumulado;
        vec[n++] = c;
    }

    // Verificamos si la generación se trunco
    *truncado = (n >= max_clientes) ? 1 : 0;
    *num_clientes = n;

    if (n > 0)
    {
        cliente_t **tmp = realloc(vec, sizeof(cliente_t *) * (size_t)n);
        if (tmp)
            vec = tmp;
    }
    else
    {
        // Si no se genera nigún cliente
        cliente_t **tmp = realloc(vec, sizeof(cliente_t *) * 1);
        if (tmp)
            vec = tmp;
    }

    return vec;
}

// Funciones auxiliares
static char *recortar(char *s)
{
    char *fin;

    // Avanzamos mientras haya espacion iniciales
    while (isspace((unsigned char)*s))
        s++;

    // Retornamos si queda vacía
    if (*s == 0)
        return s;

    // Ubicamos el último no balnco
    fin = s + strlen(s) - 1;
    while (fin > s && isspace((unsigned char)*fin))
        fin--;

    fin[1] = '\0';

    return s;
}

static int es_entero_valido(const char *s, long *salida)
{
    char *fin;

    // Reiniciamos antes de convertir
    errno = 0;

    // Convertimos a base 10
    long val = strtol(s, &fin, 10);

    // Si falla, error
    if (errno != 0 || fin == s)
        return 0;

    // Verifica los espacios
    while (*fin)
    {
        if (!isspace((unsigned char)*fin))
            return 0;
        fin++;
    }

    *salida = val;
    return 1;
}

static int es_double_valido(const char *s, double *salida)
{
    char *fin;

    // Reiniciamos antes de convertir
    errno = 0;

    // Convierte a double
    double val = strtod(s, &fin);

    // Si falla, error
    if (errno != 0 || fin == s)
        return 0;

    // Verifica los espacios
    while (*fin)
    {
        if (!isspace((unsigned char)*fin))
            return 0;
        fin++;
    }

    *salida = val;
    return 1;
}

#define LINEA_MAX 512

/*
 * @brief Lee el archivo de configuración y extrae cinco parámetros obligatorios,
 * validando formato, tipo y rango. Si la lectura y validación son correctas,
 * escribe los valores en los punteros de salida y devuelve 1. En caso de error,
 * imprime un mensaje en stderr y devuelve 0.
 *
 * @param archivo Ruta al fichero de configuración.
 * @param cajeros Puntero donde se almacenará el entero CAJEROS.
 * @param tcierre Puntero donde se almacenará el entero TCIERRE.
 * @param lambda Puntero donde se almacenará el double LAMBDA.
 * @param mu Puntero donde se almacenará el double MU.
 * @param max_clientes Puntero donde se almacenará el entero MAX_CLIENTES.
 *
 */
int leer_configuracion(const char *archivo, int *cajeros, int *tcierre,
                       double *lambda, double *mu, int *max_clientes)
{
    // Abre el archivo en modo lectura
    FILE *f = fopen(archivo, "r");

    // Verificamos si abre
    if (!f)
    {
        // Si no abre, informamos de error
        fprintf(stderr, "Error: no se pudo abrir el archivo de configuracion '%s'.\n", archivo);
        return 0;
    }

    // Verificamos que todos los parámetros estan en el archivo
    int encontrado_cajeros = 0, encontrado_tcierre = 0, encontrado_lambda = 0, encontrado_mu = 0, encontrado_max = 0;
    char linea[LINEA_MAX];
    int num_linea = 0;

    // Bucle para leer cada línea del archivo
    while (fgets(linea, sizeof(linea), f))
    {
        num_linea++;
        char *p = recortar(linea);

        // Verificamos las líneas no vacías
        if (*p == '\0')
            continue;

        // Ignoramos las líneas que empiecen con #
        if (*p == '#')
            continue;

        // Buscamos el signo igual
        char *eq = strchr(p, '=');

        // Verificamos la línea
        if (!eq)
        {
            // Informar error por falta de =
            fprintf(stderr, "Error en %s:%d: linea invalida (falta '='): '%s'\n", archivo, num_linea, p);
            fclose(f);
            return 0;
        }

        // Verificamos que no hay espacios vacíos rodeando al =
        if (eq == p || *(eq - 1) == ' ' || *(eq + 1) == ' ')
        {
            // Si hay espacios, informamos de error
            fprintf(stderr, "Error en %s:%d: formato invalido, no debe haber espacios alrededor de '=': '%s'\n", archivo, num_linea, p);
            fclose(f);
            return 0;
        }

        *eq = '\0';
        char *nombre = recortar(p);
        char *valor = recortar(eq + 1);

        // Verificamos con los nombres de los parámetros
        if (strcmp(nombre, "CAJEROS") == 0)
        {
            long v;

            // Verificamos que sea un entero
            if (!es_entero_valido(valor, &v))
            {
                // Si no es entero, informamos de error
                fprintf(stderr, "Error en %s:%d: CAJEROS debe ser entero: '%s'\n", archivo, num_linea, valor);
                fclose(f);
                return 0;
            }
            // Verificamos que sea mayor que 1
            if (v < 1)
            {
                // Si es menor, informamos error
                fprintf(stderr, "Error en %s:%d: CAJEROS debe ser >= 1: %ld\n", archivo, num_linea, v);
                fclose(f);
                return 0;
            }
            *cajeros = (int)v;
            encontrado_cajeros = 1;
        }
        else if (strcmp(nombre, "TCIERRE") == 0)
        {
            long v;

            // Verificamos que sea entero
            if (!es_entero_valido(valor, &v))
            {
                // Si no es entero, informamos error
                fprintf(stderr, "Error en %s:%d: TCIERRE debe ser entero: '%s'\n", archivo, num_linea, valor);
                fclose(f);
                return 0;
            }

            // Verificamos que sea mayor que 0
            if (v <= 0)
            {

                // Si es menor, reportamos error
                fprintf(stderr, "Error en %s:%d: TCIERRE debe ser > 0: %ld\n", archivo, num_linea, v);
                fclose(f);
                return 0;
            }
            *tcierre = (int)v;
            encontrado_tcierre = 1;
        }
        else if (strcmp(nombre, "LAMBDA") == 0)
        {
            double v;
            // Verificamos que sea numérico
            if (!es_double_valido(valor, &v))
            {
                // Si no es, reportamos error
                fprintf(stderr, "Error en %s:%d: LAMBDA debe ser numérico: '%s'\n", archivo, num_linea, valor);
                fclose(f);
                return 0;
            }
            // Verificamos que sea mayor que 0.0
            if (!(v > 0.0))
            {
                // Si es menor, reportamos error
                fprintf(stderr, "Error en %s:%d: LAMBDA debe ser > 0: %g\n", archivo, num_linea, v);
                fclose(f);
                return 0;
            }
            *lambda = v;
            encontrado_lambda = 1;
        }
        else if (strcmp(nombre, "MU") == 0)
        {
            double v;
            // Verificamos que sea numérico
            if (!es_double_valido(valor, &v))
            {
                // Si no es, reportamos error
                fprintf(stderr, "Error en %s:%d: MU debe ser numérico: '%s'\n", archivo, num_linea, valor);
                fclose(f);
                return 0;
            }
            // Verificamos que sea mayor que 0.0
            if (!(v > 0.0))
            {
                // Si es menor, reportamos error
                fprintf(stderr, "Error en %s:%d: MU debe ser > 0: %g\n", archivo, num_linea, v);
                fclose(f);
                return 0;
            }
            *mu = v;
            encontrado_mu = 1;
        }
        else if (strcmp(nombre, "MAX_CLIENTES") == 0)
        {
            long v;
            // Verificamos que sea entero
            if (!es_entero_valido(valor, &v))
            {
                // Si no es, reportamos error
                fprintf(stderr, "Error en %s:%d: MAX_CLIENTES debe ser entero: '%s'\n", archivo, num_linea, valor);
                fclose(f);
                return 0;
            }
            // Verificamos que sea mayor que 1
            if (v < 1)
            {
                // Si no es, reportamos error
                fprintf(stderr, "Error en %s:%d: MAX_CLIENTES debe ser >= 1: %ld\n", archivo, num_linea, v);
                fclose(f);
                return 0;
            }
            *max_clientes = (int)v;
            encontrado_max = 1;
        }
        else
        {
            // Si conseguimos algo desconocido, lo ignoramos
            fprintf(stderr, "Warning en %s:%d: parametro desconocido '%s' (se ignora)\n", archivo, num_linea, nombre);
        }
    }

    // Cerramos el archivo en modo lectura
    fclose(f);

    // Verificamos que encontramos todos los parámetros
    if (!encontrado_cajeros)
    {
        fprintf(stderr, "Error: falta el parametro obligatorio CAJEROS en %s\n", archivo);
        return 0;
    }
    if (!encontrado_tcierre)
    {
        fprintf(stderr, "Error: falta el parametro obligatorio TCIERRE en %s\n", archivo);
        return 0;
    }
    if (!encontrado_lambda)
    {
        fprintf(stderr, "Error: falta el parametro obligatorio LAMBDA en %s\n", archivo);
        return 0;
    }
    if (!encontrado_mu)
    {
        fprintf(stderr, "Error: falta el parametro obligatorio MU en %s\n", archivo);
        return 0;
    }
    if (!encontrado_max)
    {
        fprintf(stderr, "Error: falta el parametro obligatorio MAX_CLIENTES en %s\n", archivo);
        return 0;
    }

    return 1;
}

int main(int argc, char *argv[])
{
    // Semilla para números aleatorios
    srand(time(NULL));

    // Iniciamos los eventos
    inicializar_eventos();

    // Verificamos que se haya proporcionado el archivo de configuración
    if (argc != 2)
    {
        fprintf(stderr, "Uso: %s archivo_config.txt\n", argv[0]);
        return EXIT_FAILURE;
    }

    int cajeros, tcierre, max_clientes, truncado;
    double lambda, mu_leido; // mu_leido es el valor de MU del archivo

    // Verificamos que se pudo leer la configuración correctamente
    if (!leer_configuracion(argv[1], &cajeros, &tcierre, &lambda, &mu_leido, &max_clientes))
    {
        return EXIT_FAILURE;
    }

    // Asignar la variable global mu
    mu = mu_leido;

    // Generar los clientes
    int num_clientes;
    cliente_t **clientes = generar_clientes(lambda, tcierre, max_clientes, &num_clientes, &truncado);

    // Agregamos la llegada de cada cliente
    for (int i = 0; i < num_clientes; i++)
    {
        agregar_evento(clientes[i]->llegada, EVENTO_LLEGADA, clientes[i]->id, -1);
    }

    // Verificamos que se pudieron generar los clientes correctamente
    if (clientes == NULL)
    {
        fprintf(stderr, "Error: No se pudieron generar los clientes.\n");
        return EXIT_FAILURE;
    }

    // Inicializamos las estructuras
    inicializar_cola(max_clientes);
    inicializar_estadisticas();

    // Encolamos los clientes
    for (int i = 0; i < num_clientes; i++)
    {
        agregar_cliente(clientes[i]);
    }

    banco_cerrado = 1;

    // Creamos los hilos de los cajeros
    pthread_t hilos[cajeros];

    for (int i = 0; i < cajeros; i++)
    {
        int id = i + 1;
        if (pthread_create(&hilos[i], NULL, atender_clientes, (void *)(intptr_t)id) != 0)
        {
            // printf("DEBUG: Hilo para cajero %d creado\n", i + 1);
            fprintf(stderr, "Error al crear el hilo del cajero %d\n", id);
            banco_cerrado = 1;
            return EXIT_FAILURE;
        }
    }

    // Esperamos a que los hilos de los cajeros terminen
    for (int i = 0; i < cajeros; i++)
    {
        pthread_join(hilos[i], NULL);
    }

    imprimir_eventos_ordenados();
    liberar_eventos();

    // Obtenemos las estadísticas para el resumen final
    int atendidos = estadistica_atendidos();
    double wq_sim = estadistica_promedio_wq();
    double w_sim = estadistica_promedio_w();
    double max_espera = estadistica_max_espera();
    double ultimo_fin = estadistica_ultimo_fin();
    double rho, wq_teo, w_teo;
    int estable;
    calcular_teoricas(cajeros, lambda, mu_leido, &rho, &wq_teo, &w_teo, &estable);

    // Mostramos el resumen final
    imprimir_resumen(cajeros, tcierre, lambda, mu_leido, max_clientes,
                     atendidos, truncado, wq_sim, w_sim, max_espera, ultimo_fin,
                     rho, wq_teo, w_teo, estable);

    // Liberaramos todo
    liberar_cola();

    // Destruimos Mutex
    pthread_mutex_destroy(&estadisticas_clientes.mutex);

    // Liberamos clientes
    free(clientes);

    return EXIT_SUCCESS;
}