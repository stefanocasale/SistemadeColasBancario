Persona A:
Debe implementar las siguientes funciones tanto en banco.c, dichas funciones ya estan puestas en banco.h (los prototipos estan listos)

- leer_configuracion:
    - OBJETIVO: Leer y validar el archivo de configuración pasado como argumento, extrayendo los cinco parámetros obligatorios. El formato del archivo está definido en la sección 4 del enunciado.
    - ESPECIFICACIONES DETALLADAS: 
        - Apertura del archivo
        - Ignorar líneas vacías y comentarios
        - Formato de Parámetro: Cada línea válida debe tener PARAMETRO=VALOR sin espacios alrededor del =. Ejemplo: CAJEROS=4.
        - Parámetros requerido por el enunciado Seccion 4.2
        - Manejo de errores
        - Valores de retorno:
            - 1 : si todo es correcto (los parámetros se asignan a través de los punteros).
            - 0 : en caso de error (el programa debe terminar).

- generar_clientes:
    - OBJETIVO: Generar los clientes según un proceso de Poisson con tasa lambda, deteniéndose cuando el tiempo acumulado supere tcierre o se alcance max_clientes. Cada cliente debe tener un id secuencial y un tiempo de llegada A_i (en segundos lógicos)
    - ESPECIFICACIONES DETALLADAS:
        - Generación de números aleatorios
        - Tiempos entre llegadas (Seccion 5.1 enunciado)
        - Bucle de generación
        - Almacenamiento
        - Parámetros de salida
            - *num_clientes = cantidad de clientes generados.
            - *truncado :
                - 1 : 1 si se alcanzó max_clientes antes de superar tcierre
                - 0 : caso contrario

- calcular_teoricas:
    - OBJETIVO: Calcular las métricas del modelo M/M/c (Erlang‑C) según las fórmulas de la sección 5.4 del enunciado.
    - ESPECIFICACIONES DETALLADAS:
        - Tráfico ofrecido
        - Utilización por servidor
        - Condición de estabilidad
        - Implementación iterativa: Para evitar desbordamiento numérico
        - Tiempos promedio
        - os resultados se devuelven a través de los punteros rho, Wq_teo, W_teo y estable.

- imprimir_resumen:
    - OBJETIVO: Imprimir en stdout el resumen final con el formato exacto especificado en la sección 11 del enunciado.
    - ESPECIFICACIONES DETALLADAS:
        - Formato exacto (Parte 11 del enunciado)
        - Detalles de formato:
            - Todos los números con dos decimales a excepción de enteros y rho
            - Manejo de sistema estable
            - Cálculo de errores relativos (solo si estable)


Consideraciones adicionales para la integración
- Variables globales compartidas
    - mu (double): definida en banco.c como double mu;. El main asigna mu = mu_leido. Los hilos cajeros la usan en generar_servicio().
    - banco_cerrado (int): definida en banco.c como int banco_cerrado = 0;. El main la pone a 1 después de lanzar los hilos. Es leída por obtener_cliente() para decidir si esperar o no.

-Gestión de memoria
    - Persona A: En generar_clientes, cada cliente debe ser creado con malloc. El arreglo de punteros también debe ser reservado con malloc.
    - Persona B: En atender_clientes, se hace free(cliente) después de atenderlo. Esto libera la memoria de cada cliente individual.
    - Al final del main, se libera el arreglo de punteros con free(clientes). No se deben liberar los clientes nuevamente, pues ya están liberados.