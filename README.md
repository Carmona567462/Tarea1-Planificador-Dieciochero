# Planificador Dieciochero

Tarea 1 de Sistemas Operativos.

## Integrantes

Ricardo Vargas
Daniel Carmona

## Descripción

Este programa implementa un planificador de actividades a partir de un archivo de texto.

Cada actividad tiene un identificador, un nombre, un tiempo de ejecución y puede depender de una o más actividades anteriores.

Antes de comenzar la ejecución, el programa revisa que el archivo sea válido y que las dependencias formen un DAG, es decir, que no existan ciclos entre las actividades.

Durante la ejecución, cada actividad se realiza en un proceso hijo creado con fork(). El programa también recibe un valor K, que indica la cantidad máxima de procesos que pueden estar ejecutándose al mismo tiempo.

Además, se utilizan pipes para enviar mensajes entre los procesos y las actividades que dependen de otras.

---

## Compilación

Para compilar el programa se utiliza:
bash
make

También se puede eliminar el ejecutable generado con:
bash
make clean

El programa utiliza C++17 y se compila con las opciones indicadas en el Makefile.

---

## Ejecución

El programa se ejecuta de la siguiente forma:
bash
./planificador plan.txt K

Donde:

plan.txt es el archivo que contiene las actividades.
K es la cantidad máxima de actividades que pueden ejecutarse al mismo tiempo.

Por ejemplo:
bash
./planificador plan.txt 2

En este caso se podrán ejecutar como máximo dos procesos hijos de forma concurrente.

K debe ser un número entero mayor que cero.

---

## Formato del archivo de entrada

Cada línea del archivo debe tener el siguiente formato:
text
ID : nombre : tiempo_ms : dependencias

Ejemplo:
text
1 : prender_carbon : 500 :
2 : comprar_carne : 1200 :
3 : comprar_pan : :
4 : asar_longaniza : 800 : 1, 2
5 : armar_choripan : 250 : 3, 4
6 : servir_mesa : 100 : 5

En este ejemplo, la actividad 4 depende de las actividades 1 y 2, por lo que no puede comenzar hasta que ambas hayan terminado correctamente.

Si una actividad no tiene tiempo definido, el programa genera un tiempo aleatorio entre 100 y 5000 ms.

---

## Validaciones realizadas

Antes de ejecutar el plan se revisan distintos errores posibles.

El programa comprueba:

que el archivo pueda abrirse correctamente;
que cada línea tenga el formato esperado;
que los ID no estén repetidos;
que todas las dependencias indicadas existan;
que los tiempos sean válidos;
que el grafo de dependencias no tenga ciclos;
que K sea un número entero mayor que cero.

Si alguna de estas condiciones no se cumple, el programa muestra un mensaje de error y no comienza la ejecución.

---

## Manejo de dependencias

Las actividades son tratadas como nodos de un DAG.

Para controlar qué actividad puede ejecutarse, se guarda la cantidad de dependencias pendientes de cada una.

Cuando una actividad termina correctamente, se actualizan las actividades que dependen de ella. Si una actividad ya no tiene dependencias pendientes, queda disponible para ser ejecutada.

Para hacer estas búsquedas de forma más rápida se utilizan estructuras como:
cpp
unordered_map
vector
queue

Esto evita tener que recorrer todas las actividades cada vez que termina un proceso.

---

## Procesos y concurrencia

Cada actividad se ejecuta utilizando un proceso hijo creado con:
cpp
fork()

El proceso hijo espera durante el tiempo correspondiente a la actividad usando usleep() y después finaliza.

El proceso padre controla los procesos que terminan utilizando:
cpp
waitpid()

Se utiliza una espera bloqueante, por lo que el programa no está consultando constantemente si un hijo terminó.

La variable K controla la cantidad máxima de procesos que pueden estar activos al mismo tiempo.

Por ejemplo, si K = 2, aunque existan cinco actividades listas para comenzar, solamente dos se ejecutan al mismo tiempo. Cuando una termina, puede comenzar otra.

---

## Comunicación con pipes

Los pipes se utilizan para comunicar los procesos durante la ejecución.

Cuando un proceso hijo termina una actividad, envía un mensaje al proceso padre indicando que terminó correctamente.

Un mensaje puede tener una forma similar a:
text
Insumo completado: 1 .- prender_carbon

Después, el planificador guarda ese mensaje para las actividades que dependan de la actividad terminada.

Antes de ejecutar una actividad que tiene dependencias, se crea un pipe de entrada y se envían los mensajes correspondientes. El proceso hijo lee estos mensajes antes de comenzar su actividad.

Por ejemplo, si la actividad 4 depende de 1 y 2, primero recibe los mensajes enviados por ambas actividades y después comienza su ejecución.

Se utilizó un vector<string> insumos para guardar temporalmente estos mensajes. Esto fue útil porque una actividad puede depender de más de una actividad y, por lo tanto, puede recibir varios mensajes.

---

## Manejo de fallos

Si una actividad falla durante su ejecución, no se cancela todo el plan.

La actividad que falló se marca como fallida y se bloquean solamente las actividades que dependan directa o indirectamente de ella.

Las actividades que pertenecen a otra rama y no necesitan el resultado de la actividad fallida pueden continuar ejecutándose normalmente.

Por ejemplo:
text
1 ---> 4 ---> 5
2 ---> 4
3 ---------> 5

Si falla la actividad 1, se bloquean las actividades 4 y 5, pero las actividades 2 y 3 pueden continuar.

Para esto cada actividad puede quedar en uno de los siguientes estados:

completada;
fallida;
bloqueada.

Al finalizar, si existieron errores, el programa muestra la cantidad de actividades completadas, fallidas y bloqueadas.

---

## Ctrl+C

El programa también maneja la señal SIGINT.

Si el usuario presiona:
text
Ctrl+C

el planificador detecta la interrupción, detiene los procesos hijos que todavía se encuentran activos y utiliza waitpid() para recogerlos antes de terminar el programa.

Esto evita dejar procesos ejecutándose después de cerrar el planificador.

---

## Pruebas realizadas

Durante el desarrollo se probaron distintos casos, entre ellos:

ejecución con K = 1;
ejecución con varios procesos concurrentes;
ID repetidos;
dependencias inexistentes;
ciclos entre actividades;
tiempos vacíos;
valores incorrectos de K;
actividades con varias dependencias;
fallo de una actividad y bloqueo solamente de su rama;
interrupción mediante Ctrl+C;
planes con 10.000 actividades.

También se realizaron pruebas con una cadena de 10.000 actividades y con una gran cantidad de actividades independientes para revisar el funcionamiento del planificador con una carga mayor.

---

## Decisiones de implementación

Se decidió representar cada actividad mediante una estructura Actividad, donde se guarda su ID, nombre, tiempo, dependencias, mensajes recibidos, PID y estado.

Se utilizaron procesos en vez de hilos, ya que cada actividad debe ejecutarse mediante fork().

Para respetar el límite de concurrencia se lleva un contador de procesos activos y solo se crean nuevos procesos mientras este valor sea menor que K.

Para controlar las dependencias se utiliza una cola de actividades disponibles. Cuando una actividad termina, se actualiza la cantidad de dependencias pendientes de las actividades relacionadas.

También se utilizan mapas para encontrar actividades y procesos por su ID o PID sin tener que recorrer constantemente todo el vector.

Los pipes se crean solamente cuando son necesarios, en vez de mantener pipes abiertos para todas las actividades desde el principio. Esto permite trabajar mejor con planes grandes y evita mantener demasiados descriptores abiertos.

---

## Archivos principales

El proyecto contiene principalmente:
text
main.cpp
Makefile
plan.txt
README.md
main.cpp contiene la implementación del planificador.

Makefile permite compilar y limpiar el proyecto.

plan.txt contiene un ejemplo de las actividades que puede ejecutar el programa.

README.md contiene la explicación general del proyecto y su forma de uso.