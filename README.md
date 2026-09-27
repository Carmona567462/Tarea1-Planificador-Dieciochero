# Tarea Planificador Dieciochero

Tarea 1 de Sistemas Operativos.

## Integrantes

Ricardo Vargas
Daniel Carmona

## Descripción

El programa consiste en un planificador de actividades que lee un archivo plan.txt.

Cada actividad tiene un ID, un nombre, un tiempo de ejecución y puede depender de otras actividades.

Las actividades forman un DAG, por lo que antes de comenzar la ejecución se revisa que el archivo tenga una estructura válida.

## Compilación

Para compilar el programa se utiliza:
bash
make

Para eliminar el ejecutable generado:
bash
make clean

El proyecto utiliza C++17 y se compila con las opciones indicadas para la tarea.

## Ejecución

El programa se ejecuta de la siguiente forma:
bash
./planificador plan.txt K

Donde:

plan.txt es el archivo que contiene las actividades.
K corresponde al límite máximo de procesos que podrán ejecutarse al mismo tiempo.

## Formato de plan.txt

Cada línea del archivo tiene el siguiente formato:
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
Si una actividad no tiene un tiempo definido, el programa le asigna un tiempo aleatorio entre 100 y 5000 milisegundos.

## Funcionalidades implementadas

Hasta el momento se ha implementado:

Lectura y parseo de plan.txt.
Almacenamiento de las actividades y sus dependencias.
Generación de tiempos aleatorios cuando no se indica un tiempo.
Detección de IDs repetidos.
Validación de dependencias que no existen.
Detección de ciclos para comprobar que el plan corresponda a un DAG.
Creación inicial de procesos mediante fork().
Espera de procesos hijos utilizando waitpid().

## Decisiones de implementación

Las actividades se guardan en una estructura que contiene su ID, nombre, tiempo, dependencias y datos que serán utilizados durante la ejecución.

Antes de comenzar a ejecutar las actividades se valida el archivo para evitar problemas con IDs repetidos, dependencias inexistentes o ciclos dentro del grafo.

Para ejecutar las actividades se utilizan procesos creados mediante fork().

La espera de los procesos se realiza utilizando waitpid(), evitando realizar espera activa.

El README se irá actualizando a medida que se agreguen las demás funcionalidades del planificador.s