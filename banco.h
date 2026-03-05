#ifndef BANCO_H
#define BANCO_H

#include <stdint.h>

typedef struct cliente
{
    int id;
    double llegada; // Tiempo de llegada A_i
} cliente_t;

// Funciones para la gestión de la cola circular de clientes
void inicializar_cola(int capacidad_maxima); // Inicializa la cola circular con la capacidad dada
void agregar_cliente(cliente_t *cliente);    // Agrega un cliente a la cola circular, bloqueando si la cola está llena
cliente_t *obtener_cliente();                // Obtiene el siguiente cliente de la cola circular
void liberar_cola();                         // Libera los recursos asociados a la cola circular
void inicializar_estadisticas();             // Inicializa las estadísticas

// Funciones para la gestión de las estadísticas de los clientes atendidos
void inicializar_estadisticas();                           // Inicializa las estadísticas
void agregar_estadistica(double wq, double w, double fin); // Agrega una estadística de un cliente atendido
double estadistica_promedio_wq();                          // Devuelve el promedio de los tiempos de espera en la cola ()
double estadistica_promedio_w();                           // Devuelve el promedio de los tiempos de espera total
double estadistica_max_espera();                           // Devuelve el tiempo de espera máximo registrado
double estadistica_ultimo_fin();                           // Devuelve el tiempo de finalización del último cliente atendido
int estadistica_atendidos();                               // Devuelve el número total de clientes atendidos
void estadistica_destroy();                                // Libera los recursos asociados a las estadísticas

extern int banco_cerrado;  // Variable para indicar si el banco está cerrado
extern int mu; // Tasa de servicio (clientes por unidad de tiempo)

#endif