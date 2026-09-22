#include <iostream>
#include <string>
#include <vector>
#include <fstream>
#include <cstdlib>
#include <unistd.h>
#include <sys/wait.h>

using namespace std;

struct Actividad
{
    string id_Actividad;
    string nombre_Actividad;
    int tiempo;
    vector<string> dependencias;

    int pipe_fd[2];
    pid_t pid_hijo = -1;
    bool completada = false;
};

void inicializarPipe(Actividad& tarea) {
    if (pipe(tarea.pipe_fd) == -1) {
        exit(EXIT_FAILURE);
    }
}

void propagarMensaje(Actividad& tarea_finalizada) {
    close(tarea_finalizada.pipe_fd[0]); 
    
    string msg = "Insumo completado: " + tarea_finalizada.nombre_Actividad;
    write(tarea_finalizada.pipe_fd[1], msg.c_str(), msg.length() + 1);
    
    close(tarea_finalizada.pipe_fd[1]); 
}

void recibirInsumo(Actividad& tarea_dependencia) {
    close(tarea_dependencia.pipe_fd[1]); 
    
    char buffer[256];
    read(tarea_dependencia.pipe_fd[0], buffer, sizeof(buffer));
    
    close(tarea_dependencia.pipe_fd[0]); 
}

int main(int argc, char* argv[])
{
    if (argc != 3)
    {
        cerr << "Error: Cantidad de argumentos incorrecta" << endl;
        cerr << "Ejecuta el programa de la siguiente forma:" << endl;
        cerr << argv[0] << " <archivo.txt> <K>" << endl;
        return 1;
    }

    string nombreArchivo = argv[1];
    int k = stoi(argv[2]);
    cout << "Archivo que se va a leer: " << nombreArchivo << endl;
    cout << "Con el límite de concurrencia: " << k << endl;

    vector<Actividad> lista_actividades = {
        {"1", "prender_carbon", 2000, {}, {-1, -1}, -1, false},
        {"2", "comprar_carne", 3000, {}, {-1, -1}, -1, false}
    };

    int procesos_activos = 0;
    int tareas_completadas = 0;
    int total_tareas = lista_actividades.size();

    cout << "\n[Planificador] Iniciando asado de prueba..." << endl;

    return 0;
}