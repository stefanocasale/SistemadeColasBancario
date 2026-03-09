#ifndef BANCO_H
#define BANCO_H

#include <stdint.h>

// Estructura para representar a un cliente
typedef struct cliente
{
    int id;
    double llegada; // Tiempo de llegada A_i
} cliente_t;

// Tipos de eventos
typedef enum
{
    EVENTO_LLEGADA,
    EVENTO_INICIO,
    EVENTO_FIN
} tipo_evento_t;

// Estructura de un evento
typedef struct evento
{
    double tiempo;
    tipo_evento_t tipo;
    int id_cliente;
    int id_cajero; // -1 para llegadas
} evento_t;

// Funciones para manejo de eventos
void inicializar_eventos();
void agregar_evento(double tiempo, tipo_evento_t tipo, int id_cliente, int id_cajero);
void imprimir_eventos_ordenados();
void liberar_eventos();

// Funciones para la gestión de clientes
void inicializar_cola(int capacidad_maxima);             // Inicializa la cola circular con la capacidad dada
void agregar_cliente(cliente_t *cliente);                // Agrega un cliente a la cola circular, bloqueando si la cola está llena
cliente_t *obtener_cliente();                            // Obtiene el siguiente cliente de la cola circular
void liberar_cola();                                     // Libera los recursos asociados a la cola circular
void *atender_clientes(void *arg);                       // Función para que los cajeros atiendan a los clientes
cliente_t **generar_clientes(double lambda, int tcierre, // Genera un arreglo de clientes
                             int max_clientes, int *num_clientes, int *truncado);

// Funciones para la gestión de las estadísticas
void inicializar_estadisticas();                           // Inicializa las estadísticas
void agregar_estadistica(double wq, double w, double fin); // Agrega una estadística de un cliente atendido
double estadistica_promedio_wq();                          // Devuelve el promedio de los tiempos de espera en la cola ()
double estadistica_promedio_w();                           // Devuelve el promedio de los tiempos de espera total
double estadistica_max_espera();                           // Devuelve el tiempo de espera máximo registrado
double estadistica_ultimo_fin();                           // Devuelve el tiempo de finalización del último cliente atendido
int estadistica_atendidos();                               // Devuelve el número total de clientes atendidos
void estadistica_destroy();                                // Libera los recursos asociados a las estadísticas
void calcular_teoricas(int cajeros, double lambda,         // Calcula las métricas teóricas M/M/c y dice si el sistema es estable
                       double mu, double *rho, double *Wq_teo, double *W_teo, int *estable);

// Funciones para la gestion del archivo configuración
int leer_configuracion(const char *archivo, int *cajeros, int *tcierre, // Lee la configuración del archivo y asigna los valores
                       double *lambda, double *mu, int *max_clientes);

// Función para imprimir el resumen final
void imprimir_resumen(int cajeros, int tcierre, double lambda, double mu,
                      int max_clientes, int clientes_atendidos, int truncado,
                      double Wq_sim, double W_sim, double max_espera, double tiempo_ultimo,
                      double rho, double Wq_teo, double W_teo, int estable);

// Variables globales
extern int banco_cerrado; // Indica si el banco está cerrado
extern double mu;         // Tasa de servicio (clientes por unidad de tiempo)

#endif