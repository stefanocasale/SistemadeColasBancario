División del Trabajo – Proyecto 2: Simulación Concurrente de un Sistema de Colas Bancario
Hemos analizado el enunciado y proponemos una división del trabajo en dos partes independientes, con interfaces claramente definidas. Cada persona podrá desarrollar y probar su módulo por separado, y al final se integrarán sin conflictos. A continuación se detallan las responsabilidades de cada integrante.

Persona A – Configuración, Generación de Clientes y Métricas Teóricas
Objetivo: Implementar todo lo relacionado con la entrada del programa (archivo de configuración), la generación de los clientes (tiempos de llegada) y los cálculos teóricos del modelo M/M/c. Además, se encargará de imprimir el resumen final.

Módulos a desarrollar
Lectura y validación del archivo de configuración

Leer el archivo .txt pasado como argumento.

Ignorar líneas vacías y comentarios (inician con #).

Parsear líneas con formato PARAMETRO=VALOR (sin espacios alrededor del =).

Verificar que existan los cinco parámetros obligatorios: CAJEROS, TCIERRE, LAMBDA, MU, MAX_CLIENTES.

Validar que los valores sean del tipo correcto (enteros para los primeros y último, double para LAMBDA y MU) y cumplan las restricciones (≥1, >0, etc.).

Si hay error, mostrar mensaje descriptivo en stderr y terminar el programa.

Devolver los valores en una estructura o mediante parámetros por referencia.

Prueba independiente: Crear un pequeño programa que lea distintos archivos (válidos e inválidos) y verifique que se detectan los errores.

Generación de los clientes (tiempos de llegada)

Implementar la generación de números aleatorios con rand() y srand(time(NULL)).

Generar los tiempos entre llegadas exponenciales usando la fórmula:
T_i = -log(U) / LAMBDA, donde U es uniforme en (0,1).

Acumular el tiempo hasta superar TCIERRE o hasta alcanzar MAX_CLIENTES.

Almacenar cada cliente en un arreglo dinámico con su id (secuencial) y tiempo de llegada A_i.

Indicar si se alcanzó el límite MAX_CLIENTES (truncamiento).

Devolver el arreglo de clientes, la cantidad generada y la bandera de truncamiento.

Prueba independiente: Generar clientes con parámetros conocidos y verificar que la cantidad y los tiempos sean razonables (por ejemplo, media de llegadas ≈ λ × Tcierre).

Cálculo de métricas teóricas (Erlang-C)

Calcular el tráfico ofrecido a = λ / μ.

Calcular la utilización por servidor ρ = a / c.

Si ρ ≥ 1, marcar el sistema como inestable y no calcular más.

En caso contrario, calcular el factor de Erlang-C (probabilidad de espera):

text
     (a^c / c!)
C = ----------------
    sum_{k=0}^{c-1} (a^k / k!) + (a^c / c!) * (1/(1-ρ))
Nota: Implementar de forma iterativa para evitar desbordamiento (usar dobles y acumular términos).

Calcular Wq_teo = C / (c*μ - λ) y W_teo = Wq_teo + 1/μ.

Devolver estos valores junto con ρ y la bandera de estabilidad.

Prueba independiente: Comparar con valores conocidos de la literatura o con calculadoras online de M/M/c.

Impresión del resumen final

Recibir los parámetros, las estadísticas simuladas (desde la Persona B) y los resultados teóricos.

Imprimir en stdout el formato exacto especificado en la sección 11 del enunciado.

Incluir el cálculo de errores relativos si el sistema es estable.

Prueba independiente: Llamar a la función con datos de ejemplo y verificar que la salida coincide con el formato.

Persona B – Simulación Concurrente (Cola, Hilos, Sincronización)
Objetivo: Implementar toda la lógica concurrente: la cola compartida, los hilos cajeros, la sincronización con mutex y variables de condición, y la recolección de estadísticas simuladas. También escribirá el flujo principal (main) que orquesta la simulación.

Módulos a desarrollar
Estructuras de datos compartidas 

Definir la estructura cliente_t (con id, llegada, y opcionalmente inicio y fin para depuración, aunque estos los calcula cada cajero).

Definir la estructura estadisticas_t que contendrá acumuladores (suma de Wq, suma de W, máximo de espera, tiempo del último cliente atendido, contador de atendidos) y un mutex para protegerla.

Nota: Estas definiciones deben estar en un archivo .h común (junto con las de Persona A) para que ambos las conozcan.

Cola FIFO concurrente

Elegir una implementación (lista enlazada simple o arreglo circular).

Proveer funciones:

void cola_init(): inicializa la cola y su mutex.

void cola_push(cliente_t *cli): inserta al final (protegido con mutex).

cliente_t* cola_pop(): extrae del frente; si está vacía, retorna NULL (protegido).

void cola_destroy(): libera memoria y destruye el mutex.

La cola debe ser utilizada tanto por el hilo principal (para encolar clientes) como por los cajeros (para extraer).

Prueba independiente: Crear un programa de prueba que lance varios hilos que hagan pushes y pops concurrentemente y verificar que no se pierdan elementos.

Variables de sincronización globales

pthread_mutex_t mutex_cola (ya incluido en la cola, pero puede ser separado).

pthread_cond_t cond_cola: para que los cajeros esperen cuando la cola está vacía.

int banco_cerrado: indicador de que no llegarán más clientes.

Lógica de los hilos cajeros

Función void* atender_clientes(void *arg):

Cada hilo tiene una variable local tiempo_libre (inicializada en 0.0).

Bucle mientras (!banco_cerrado o haya clientes en cola):

Bloquear el mutex de la cola.

Mientras la cola esté vacía y banco_cerrado == 0, esperar en cond_cola.

Si la cola no está vacía, extraer un cliente.

Si la cola está vacía pero banco_cerrado == 1, salir del bucle (el hilo termina).

Liberar el mutex.

Calcular B = max(cliente->llegada, tiempo_libre).

Generar tiempo de servicio S = -log(U) / MU (con U uniforme).

Calcular F = B + S.

Actualizar tiempo_libre = F.

Imprimir eventos de inicio y fin (proteger la impresión con un mutex global de salida o con el mismo mutex de la cola para evitar intercalado).

Actualizar estadísticas:

Wq = B - cliente->llegada

W = F - cliente->llegada

Acumular en las estadísticas globales (con su propio mutex) y actualizar máximo.

Liberar el cliente (si se asignó dinámicamente).

Prueba independiente: Crear un main de prueba que inicialice la cola con algunos clientes (con tiempos de llegada fijos), lance los hilos y observe que los tiempos de atención se calculan correctamente.

Estadísticas simuladas

Funciones para actualizar y consultar:

void stats_init(): inicializa acumuladores y mutex.

void stats_agregar(double wq, double w, double fin): suma, actualiza máximo y registra el último tiempo de fin.

double stats_promedio_wq(), double stats_promedio_w(), double stats_max_espera(), double stats_ultimo_fin(), int stats_atendidos().

Prueba independiente: Llamar desde varios hilos y verificar que los resultados sean consistentes.

Flujo principal (main)

Llamar a la función de Persona A para leer la configuración.

Llamar a la función de Persona A para generar los clientes (obtener arreglo y cantidad).

Inicializar cola, estadísticas y variables de sincronización.

Encolar todos los clientes generados (usando cola_push).

Crear los hilos cajeros (pthread_create).

Una vez creados, establecer banco_cerrado = 1 y hacer pthread_cond_broadcast (esto está en el enunciado, pero en realidad los clientes ya están encolados antes de lanzar los hilos; la lógica de terminación se activa después de que todos los clientes están en cola y los hilos ya están corriendo. Otra opción es lanzar los hilos, luego encolar, y al final cerrar. Hay que decidir. Lo típico es: lanzar hilos, luego el main genera y encola, y cuando termina de encolar, pone banco_cerrado=1 y broadcast. Así los hilos trabajan mientras se encolan. Pero el enunciado dice: "El hilo principal (main) genera todos los clientes, los encola en orden de llegada y luego lanza los hilos cajeros." Eso es más simple: primero se generan y encolan todos, luego se lanzan los hilos. Entonces no es necesario broadcast para despertar porque al empezar ya hay clientes. Pero luego cuando se acaben, los hilos deben esperar a que lleguen más? No, como ya no llegarán más, deben terminar. El mecanismo de terminación es: los hilos, al ver la cola vacía y banco_cerrado=1, terminan. Pero si se lanzan después de encolar, al principio hay clientes, luego cuando se acaben, la cola queda vacía y banco_cerrado sigue siendo 0 (porque aún no se ha puesto a 1). Entonces se quedarían esperando en la condición. Para evitarlo, después de lanzar los hilos, el main debe poner banco_cerrado=1 y hacer broadcast. Eso es lo que dice la sección 6.6: después de encolar todos los clientes y lanzar los hilos, se hace eso. Así que el orden correcto es: generar y encolar, luego lanzar hilos, luego banco_cerrado=1 y broadcast.

Esperar a que todos los hilos terminen con pthread_join.

Obtener las estadísticas simuladas.

Llamar a la función de Persona A para calcular las teóricas (pasando los parámetros).

Llamar a la función de resumen de Persona A con todos los datos.

Liberar recursos (cola, estadísticas, etc.).

Prueba independiente: Usar un generador de clientes dummy (por ejemplo, un arreglo fijo) para verificar que la simulación produce resultados coherentes y que los hilos terminan correctamente.