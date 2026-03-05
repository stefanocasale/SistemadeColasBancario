#División del trabajo en equipo
He analizado el enunciado y propongo la siguiente separación de responsabilidades, pensada para que ambos tengan una carga equilibrada y puedan trabajar en paralelo con una interfaz clara entre sus partes.

#Persona A: Generación de clientes, configuración y métricas teóricas
Tareas principales:

1. Lectura y validación del archivo de configuración

    - Abrir el archivo, parsear líneas, ignorar comentarios y blancos.

    - Verificar que existan todos los parámetros requeridos y que sus valores sean del tipo correcto y cumplan restricciones.

    - Manejar errores con mensajes descriptivos en stderr y terminar el programa si es necesario.

2. Definición de estructuras de datos base (en coordinación con la Persona B)

    - Estructura cliente: campos llegada (double), inicio (double), fin (double), id (int).

    - Estructura estadisticas_globales: suma de tiempos de espera, suma de tiempos en sistema, máximo tiempo de espera, contador de clientes atendidos, etc.

    - Posiblemente la estructura de la cola (si se decide implementarla como lista enlazada, por ejemplo) – aunque la implementación concreta la hará la Persona B, ambos deben acordar la interfaz.

3. Generación de los clientes

    - Inicializar la semilla de números aleatorios con srand(time(NULL)).

    - Implementar el bucle que genera tiempos de llegada exponenciales usando -log(rand()/RAND_MAX) / LAMBDA.

    - Acumular el tiempo hasta superar TCIERRE o alcanzar MAX_CLIENTES.

    - Almacenar cada cliente generado en una estructura que luego será puesta en la cola compartida.

    Nota: como la cola aún no está disponible (la hará B), podrías generar un array temporal de clientes y luego pasarlo a B para que los encole, o bien B te provee una función encolar_cliente que uses durante la generación. Discutiremos esto más adelante.

4. Cálculo de métricas teóricas (Erlang-C)

Implementar las fórmulas de la sección 5.4: tráfico ofrecido a = λ/μ, utilización ρ = a / c, factor de Erlang-C, Wq_teo y W_teo.

- Manejar el caso ρ ≥ 1 (sistema inestable).

- Al final, comparar con los resultados simulados que le proporcione la Persona B y calcular errores relativos.

- Generación del resumen final

- Imprimir en stdout el bloque de salida obligatorio (sección 11), combinando los parámetros leídos, las estadísticas simuladas (de B) y los cálculos teóricos.

#Persona B: Simulación concurrente (hilos, cola, sincronización)
Tareas principales:

- Implementación de la cola compartida FIFO

- Elegir una estructura de datos (lista enlazada, arreglo circular, etc.) que soporte operaciones push (insertar al final) y pop (extraer del frente).

- Proteger todas las operaciones con un pthread_mutex_t.

- Proveer funciones para que la Persona A pueda encolar clientes (probablemente llamadas desde el hilo principal).

- Variables de sincronización y estado global

- Declarar pthread_mutex_t mutex_cola y pthread_cond_t cond_cola.

- Variable banco_cerrado (inicialmente 0) que indica que no habrá más clientes.

- Estadísticas compartidas (las definidas con A) que se actualizarán durante la simulación.

- Lógica de los hilos cajeros

- En main, crear CAJEROS hilos que ejecuten la función atender_clientes.

- Cada hilo mantiene una variable local tiempo_libre (inicializada en 0.0) que representa el momento en que el cajero estará disponible.

- Dentro del hilo:

- Entrar en un bucle mientras el banco no esté cerrado o haya clientes en cola.

- Bloquear el mutex, esperar en la condición si la cola está vacía y el banco aún no cierra.

- Al despertar, extraer un cliente de la cola (si hay) y actualizar estadísticas de manera atómica.

- Liberar el mutex.

- Calcular B = max(cliente->llegada, tiempo_libre), luego F = B + S (con S generado exponencialmente con MU).

- Actualizar tiempo_libre = F.

- Imprimir los eventos de inicio y fin (protegiendo la salida con el mutex o con otro mecanismo para evitar entremezclado).

- Acumular en las estadísticas globales (con mutex) los valores de Wq y W y actualizar el máximo.

- Mecanismo de terminación ordenada

- Después de que la Persona A haya generado todos los clientes y los haya encolado, el hilo principal debe establecer banco_cerrado = 1 y hacer pthread_cond_broadcast para despertar a todos los cajeros que estén esperando.

- Luego hacer pthread_join de cada cajero.

- Recolección de estadísticas simuladas

- Una vez que todos los hilos han terminado, el hilo principal (o una función de B) debe calcular los promedios (Wq_sim, W_sim) y el máximo a partir de las acumulaciones globales.

- Pasar estos valores a la Persona A para que los incluya en el resumen.

- Integración y dependencias
Interfaz común: Ambos deben acordar los nombres y tipos de las estructuras y funciones que compartirán. Por ejemplo, un archivo banco.h con:

c
typedef struct cliente cliente_t;
typedef struct estadisticas estadisticas_t;
void encolar_cliente(cliente_t *cli);  // implementada por B
void obtener_estadisticas(...);        // para que A lea los resultados
Flujo principal (main): Podría ser escrito por cualquiera, pero idealmente lo coordinan juntos. El flujo sería:

Leer configuración (A).
Inicializar estructuras (B).
Generar clientes y encolarlos (A usa funciones de B).
Crear hilos (B).
Esperar terminación (B).
Calcular teóricas y mostrar resumen (A con datos de B).