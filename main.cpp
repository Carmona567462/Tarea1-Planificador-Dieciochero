#include <iostream>
#include <string>
#include <vector>
#include <fstream>
#include <cstdlib>
#include <sstream>
#include <random>
#include <unistd.h>
#include <sys/wait.h>
#include <unordered_set>
#include <unordered_map>
#include <queue>
#include <csignal>

using namespace std;

volatile sig_atomic_t interrupcionSolicitada = 0;

void manejarSigint(int)
{
    interrupcionSolicitada = 1;
}

bool configurarSigint()
{
    struct sigaction accion{};
    accion.sa_handler = manejarSigint;
    sigemptyset(&accion.sa_mask);
    accion.sa_flags = 0;

    if (sigaction(SIGINT, &accion, nullptr) == -1)
    {
        cerr << "Error: no se pudo configurar el SIGINT." << endl;
        return false;
    }

    return true;
}

struct Actividad
{
    string id_Actividad;
    string nombre_Actividad;
    int tiempo;
    vector<string> dependencias;
    vector<string> insumos;

    int pipe_fd[2];
    int pipe_entrada[2] = {-1, -1};

    pid_t pid_hijo = -1;
    bool completada = false;
    bool fallida = false;
    bool bloqueada = false;
};

string limpiarEspacios(const string& texto)
{
    size_t inicio = texto.find_first_not_of(" \t\r\n");

    if (inicio == string::npos)
    {
        return "";
    }

    size_t fin = texto.find_last_not_of(" \t\r\n");

    return texto.substr(inicio, fin - inicio + 1);
}

bool cargarPlan(const string& nombreArchivo, vector<Actividad>& lista_actividades)
{
    ifstream archivo(nombreArchivo);

    if (!archivo.is_open())
    {
        cerr << "Error: no se pudo abrir el archivo "
             << nombreArchivo << endl;

        return false;
    }

    random_device rd;
    mt19937 generador(rd());
    uniform_int_distribution<int> tiempoAleatorio(100, 5000);

    string linea;
    int numeroLinea = 0;

    while (getline(archivo, linea))
    {
        numeroLinea++;

        if (limpiarEspacios(linea).empty())
        {
            continue;
        }

        size_t separador1 = linea.find(':');
        size_t separador2 = linea.find(':', separador1 + 1);
        size_t separador3 = linea.find(':', separador2 + 1);

        if (separador1 == string::npos ||
            separador2 == string::npos ||
            separador3 == string::npos)
        {
            cerr << "Error en la linea " << numeroLinea
                 << ": formato incorrecto." << endl;

            return false;
        }

        string campoId =
            limpiarEspacios(linea.substr(0, separador1));

        string campoNombre =
            limpiarEspacios(
                linea.substr(
                    separador1 + 1,
                    separador2 - separador1 - 1
                )
            );

        string campoTiempo =
            limpiarEspacios(
                linea.substr(
                    separador2 + 1,
                    separador3 - separador2 - 1
                )
            );

        string campoDependencias =
            limpiarEspacios(linea.substr(separador3 + 1));

        if (campoId.empty() || campoNombre.empty())
        {
            cerr << "Error en la linea " << numeroLinea
                 << ": ID o nombre vacio." << endl;

            return false;
        }

        Actividad actividad{};

        actividad.id_Actividad = campoId;
        actividad.nombre_Actividad = campoNombre;

        if (campoTiempo.empty())
        {
            actividad.tiempo = tiempoAleatorio(generador);
        }
        else
        {
            try
            {
                size_t posicion;

                actividad.tiempo = stoi(campoTiempo, &posicion);

                if (posicion != campoTiempo.size() ||
                    actividad.tiempo <= 0)
                {
                    cerr << "Error en la linea " << numeroLinea
                         << ": tiempo invalido." << endl;

                    return false;
                }
            }
            catch (...)
            {
                cerr << "Error en la linea " << numeroLinea
                     << ": tiempo invalido." << endl;

                return false;
            }
        }

        if (!campoDependencias.empty())
        {
            stringstream ss(campoDependencias);
            string dependencia;

            while (getline(ss, dependencia, ','))
            {
                dependencia = limpiarEspacios(dependencia);

                if (!dependencia.empty())
                {
                    actividad.dependencias.push_back(dependencia);
                }
            }


        }

        actividad.pipe_fd[0] = -1;
        actividad.pipe_fd[1] = -1;
        actividad.pid_hijo = -1;
        actividad.completada = false;

        lista_actividades.push_back(actividad);

    }

    archivo.close();

    return true;
}

bool validarIdsUnicos(const vector<Actividad>& lista_actividades)
{
    unordered_set<string> idsEncontrados;

    for (const Actividad& actividad : lista_actividades)
    {
        if (idsEncontrados.find(actividad.id_Actividad) != idsEncontrados.end())
        {

            cerr << "Error: el ID " << actividad.id_Actividad
                 << " esta repetido." << endl;

            return false;
        }

        idsEncontrados.insert(actividad.id_Actividad);

    }

    return true;
}

bool validarDependencias(const vector<Actividad>& lista_actividades)
{
    unordered_set<string> idsExistentes;

    for (const Actividad& actividad : lista_actividades)
    {
        idsExistentes.insert(actividad.id_Actividad);
    }

    for (const Actividad& actividad : lista_actividades)
    {
        for (const string& dependencia : actividad.dependencias)
        {
            if (idsExistentes.find(dependencia) == idsExistentes.end())
            {
                cerr << "Error: la actividad "
                     << actividad.id_Actividad
                     << " depende del ID "
                     << dependencia
                     << ", pero ese ID no existe."
                     << endl;

                return false;
            }
        }
    }

    return true;
}

bool validarSinCiclos(const vector<Actividad>& lista_actividades)
{
    unordered_map<string, int> dependenciasPendientes;
    unordered_map<string, vector<string>> dependientes;

    for (const Actividad& actividad : lista_actividades)
    {
        dependenciasPendientes[actividad.id_Actividad] =
            actividad.dependencias.size();

        for (const string& dependencia : actividad.dependencias)
        {
            dependientes[dependencia].push_back(actividad.id_Actividad);
        }
    }

    queue<string> actividadesDisponibles;

    for (const Actividad& actividad : lista_actividades)
    {
        if (dependenciasPendientes[actividad.id_Actividad] == 0)
        {
            actividadesDisponibles.push(actividad.id_Actividad);
        }
    }

    int actividadesProcesadas = 0;

    while (!actividadesDisponibles.empty())
    {
        string idActual = actividadesDisponibles.front();
        actividadesDisponibles.pop();

        actividadesProcesadas++;

        for (const string& idDependiente : dependientes[idActual])
        {
            dependenciasPendientes[idDependiente]--;

            if (dependenciasPendientes[idDependiente] == 0)
            {
                actividadesDisponibles.push(idDependiente);
            }
        }
    }

    if (actividadesProcesadas != static_cast<int>(lista_actividades.size()))
    {
        cerr << "Error: el plan contiene un ciclo y no representa un DAG."
             << endl;

        return false;
    }

    return true;
}

bool inicializarPipe(Actividad& actividad)
{
    if (pipe(actividad.pipe_fd) == -1)
    {
        cerr << "Error: no se pudo crear el pipe para la actividad "
             << actividad.id_Actividad << endl;

        return false;
    }

    return true;
}

bool propagarMensaje(Actividad& actividad)
{
    string mensaje = "Insumo completado: "
                   + actividad.id_Actividad
                   + " .- "
                   + actividad.nombre_Actividad;

    if (mensaje.size() > 255)
    {
        mensaje.resize(255);
    }

    ssize_t bytesEscritos =
        write(actividad.pipe_fd[1],
              mensaje.c_str(),
              mensaje.size() + 1);

    if (bytesEscritos == -1)
    {
        cerr << "Error: no se pudo enviar el mensaje de la actividad "
             << actividad.id_Actividad << endl;

        return false;
    }

    return true;
} 

string recibirInsumo(Actividad& actividad)
{
    char buffer[256] = {};

    ssize_t bytesLeidos =
        read(actividad.pipe_fd[0],
             buffer,
             sizeof(buffer) - 1);

    close(actividad.pipe_fd[0]);
    actividad.pipe_fd[0] = -1;

    if (bytesLeidos <= 0)
    {
        return "";
    }

    buffer[bytesLeidos] = '\0';

    return string(buffer);
}

pid_t crearProcesoActividad(Actividad& actividad)
{
    if (!inicializarPipe(actividad))
    {
        return -1;
    }

    if (!actividad.dependencias.empty())
    {
        if (actividad.insumos.size() != actividad.dependencias.size())
        {
            cerr << "Error: faltan insumos para iniciar la actividad " << actividad.id_Actividad << endl;

            close(actividad.pipe_fd[0]);
            close(actividad.pipe_fd[1]);

            actividad.pipe_fd[0] = -1;
            actividad.pipe_fd[1] = -1;
            return -1;
        }
        if (pipe(actividad.pipe_entrada) == -1)
        {
            cerr << "Error: no se pudo crear el pipe de entrada para la actividad " << actividad.id_Actividad << endl;

            close(actividad.pipe_fd[0]);
            close(actividad.pipe_fd[1]);

            actividad.pipe_fd[0] = -1;
            actividad.pipe_fd[1] = -1;
            return -1;
        }
    }

    pid_t pid = fork();

    if (pid < 0)
    {
        cerr << "Error: no se pudo crear el proceso para la actividad "
             << actividad.id_Actividad
             << endl;

        close(actividad.pipe_fd[0]);
        close(actividad.pipe_fd[1]);

        actividad.pipe_fd[0] = -1;
        actividad.pipe_fd[1] = -1;

        if (actividad.pipe_entrada[0] != -1)
        {
            close(actividad.pipe_entrada[0]);
            actividad.pipe_entrada[0] = -1;
        }

        if (actividad.pipe_entrada[1] != -1)
        {
            close(actividad.pipe_entrada[1]);
            actividad.pipe_entrada[1] = -1;
        }

        return -1;
    }

    if (pid == 0)
    {
        signal(SIGINT, SIG_IGN);

        close(actividad.pipe_fd[0]);

        if (actividad.pipe_entrada[1] != -1)
        {
            close(actividad.pipe_entrada[1]);
        }

        for (size_t i = 0; i < actividad.dependencias.size(); i++)
        {
            char buffer[256] = {};

            ssize_t bytesLeidos =
                read(
                    actividad.pipe_entrada[0],
                    buffer,
                    sizeof(buffer));

            if (bytesLeidos <= 0)
            {
                cerr << "[PIPE ]  Actividad "
                     << actividad.id_Actividad
                     << " - error al recibir insumo"
                     << endl;
                
                if (actividad.pipe_entrada[0] != -1)
                {
                    close(actividad.pipe_entrada[0]);
                }

                close(actividad.pipe_fd[1]);

                _exit(EXIT_FAILURE);
            }

            cout << "[PIPE ]  Actividad "
                 << actividad.id_Actividad
                 << " - recibio: "
                 << buffer
                 << endl;
        }

        if (actividad.pipe_entrada[0] != -1)
        {
            close(actividad.pipe_entrada[0]);
        }

        cout << "[HIJO ]  Actividad "
             << actividad.id_Actividad
             << " - iniciada - "
             << actividad.nombre_Actividad
             << endl;

        usleep(actividad.tiempo * 1000);

        cout << "[HIJO ]  Actividad "
             << actividad.id_Actividad
             << " - terminada"
             << endl;

        if (!propagarMensaje(actividad))
        {
            close(actividad.pipe_fd[1]);

            _exit(EXIT_FAILURE);
        }

        close(actividad.pipe_fd[1]);

        _exit(EXIT_SUCCESS);
    }

    close(actividad.pipe_fd[1]);
    actividad.pipe_fd[1] = -1;

    if (actividad.pipe_entrada[0] != -1)
    {
        close(actividad.pipe_entrada[0]);
        actividad.pipe_entrada[0] = -1;
    }

    for (const string& mensaje : actividad.insumos)
    {
      char buffer[256] = {};
      
      snprintf(buffer, sizeof(buffer), "%s", mensaje.c_str());

      ssize_t bytesEscritos = write(actividad.pipe_entrada[1], buffer, sizeof(buffer));

      if (bytesEscritos == -1)
      {
        cerr << "Error: no se pudo enviar un insumo a la actividad " << actividad.id_Actividad << endl;

        kill(pid, SIGTERM);
        waitpid(pid, nullptr, 0);

        if (actividad.pipe_entrada[1] != -1)
        {
            close(actividad.pipe_entrada[1]);
            actividad.pipe_entrada[1] = -1;
        }
        if (actividad.pipe_fd[0] != -1)
        {
            close(actividad.pipe_fd[0]);
            actividad.pipe_fd[0] = -1;
        }

        return -1;

      }
    }

    if (actividad.pipe_entrada[1] != -1)
    {
        close(actividad.pipe_entrada[1]);
        actividad.pipe_entrada[1] = -1;
    }

    actividad.pid_hijo = pid;

    cout << "[PADRE]  Actividad " << actividad.id_Actividad << " - proceso " << pid << " creado" << endl;

    return pid;
}

bool dependenciasCompletadas(
    const Actividad& actividad,
    const vector<Actividad>& lista_actividades)
{
    for (const string& dependencia : actividad.dependencias)
    {
        bool encontrada = false;

        for (const Actividad& otraActividad : lista_actividades)
        {
            if (otraActividad.id_Actividad == dependencia)
            {
                encontrada = true;

                if (!otraActividad.completada)
                {
                    return false;
                }

                break;
            }
        }

        if (!encontrada)
        {
            return false;
        }
    }

    return true;
}

int buscarActividadPorPid(
    const vector<Actividad>& lista_actividades,
    pid_t pid)
{
    for (size_t i = 0; i < lista_actividades.size(); i++)
    {
        if (lista_actividades[i].pid_hijo == pid)
        {
            return static_cast<int>(i);
        }
    }

    return -1;
}

bool guardarInsumoEnDependientes(const Actividad& actividad_finalizada,const string& mensaje, vector<Actividad>& lista_actividades, const vector<int>& posicionesDependientes)
{
        for (int posicion : posicionesDependientes)
        {
            Actividad& actividad = lista_actividades[posicion];

            if (actividad.bloqueada || actividad.fallida)
            {
                continue;
            }
            actividad.insumos.push_back(mensaje);

            cout << "La actividad " << actividad.id_Actividad << " recibio un insumo desde la actividad " << actividad_finalizada.id_Actividad << endl;
        }

    return true;
}

void cerrarPipesEntrada(vector<Actividad>& lista_actividades)
{
    for (Actividad& actividad : lista_actividades)
    {
        if (actividad.pipe_entrada[0] != -1)
        {
            close(actividad.pipe_entrada[0]);
            actividad.pipe_entrada[0] = -1;
        }

        if (actividad.pipe_entrada[1] != -1)
        {
            close(actividad.pipe_entrada[1]);
            actividad.pipe_entrada[1] = -1;
        }
    }
}

int bloquearDependientes(int posicionFallida, vector<Actividad>& lista_actividades, const vector<vector<int>>& dependientes)
{
    int cantidadBloqueadas = 0;

    queue<int> actividadesPorBloquear;

    for (int posicion : dependientes[posicionFallida])
    {
        actividadesPorBloquear.push(posicion);
    }

    while (!actividadesPorBloquear.empty())
    {
        int posicion = actividadesPorBloquear.front();
        actividadesPorBloquear.pop();

        Actividad& actividad = lista_actividades[posicion];

        if (actividad.bloqueada || actividad.fallida || actividad.completada)
        {
            continue;
        }

        actividad.bloqueada = true;
        cantidadBloqueadas++;

        cout << "[ERROR]  Actividad " << actividad.id_Actividad << " - bloqueada por fallo de una dependecia " << endl;

        for (int posicionDependiente : dependientes[posicion])
        {
            actividadesPorBloquear.push(posicionDependiente);
        }
    }

    return cantidadBloqueadas;
    
}

void abortarProcesosActivos(vector<Actividad>& lista_actividades)
{
    cout << endl;
    cout << "SEREMI, Ctrl+C detectado. Abortando actividades activas. " << endl;

    for (Actividad& actividad : lista_actividades)
    {
        if (actividad.pid_hijo > 0 && !actividad.completada && !actividad.fallida)
        {
            kill(actividad.pid_hijo, SIGTERM);
        }
    }
    for (Actividad& actividad :lista_actividades)
    {
        if (actividad.pid_hijo > 0 && !actividad.completada && !actividad.fallida)
        {
            waitpid(actividad.pid_hijo, nullptr, 0);

            if (actividad.pipe_fd[0] != -1)
            {
                close(actividad.pipe_fd[0]);
                actividad.pipe_fd[0] = -1;
            }
            actividad.bloqueada = true;
            actividad.pid_hijo =-1;
        }
    }

    cout << "SEREMI, todas las actividades activas fueron detenidas." << endl;
}

bool ejecutarPlan(vector<Actividad>& lista_actividades, int k)
{
    int procesos_activos = 0;
    int tareas_resueltas = 0;
    int total_tareas = static_cast<int>(lista_actividades.size());

    unordered_map<string, int> posicionPorId;

    for (int i = 0; i <total_tareas; i++)
    {
        posicionPorId[lista_actividades[i].id_Actividad] = i;
    }

    vector<int> dependenciasPendientes(total_tareas, 0);
    vector<vector<int>> dependientes(total_tareas);

    for(int i = 0; i < total_tareas; i++)
    {
        dependenciasPendientes[i] = static_cast <int>(lista_actividades[i].dependencias.size());

        for(const string& dependencia : lista_actividades[i].dependencias)
        {
            int posicionDependencia = posicionPorId[dependencia];
            dependientes[posicionDependencia].push_back(i);
        }
    }
    queue<int> actividadesDisponibles;

    for(int i = 0; i < total_tareas; i++)
    {
        if (dependenciasPendientes[i] == 0)
        {
            actividadesDisponibles.push(i);
        }
    }
    unordered_map<pid_t, int> posicionPorPid;

    while (tareas_resueltas < total_tareas)
    {
        if (interrupcionSolicitada)
        {
            abortarProcesosActivos(lista_actividades);
            return false;
        }

        while (procesos_activos < k && !actividadesDisponibles.empty())
        {
            int posicion = actividadesDisponibles.front();
            actividadesDisponibles.pop();
            Actividad& actividad = lista_actividades[posicion];

            if (actividad.completada || actividad.fallida || actividad.bloqueada)
            {
                continue;
            }

            pid_t pid = crearProcesoActividad(actividad);
            
            if (pid == -1)
            {
                cerr << "Error al crear un proceso." << endl;
                return false;
            }
            posicionPorPid[pid] = posicion;
            procesos_activos++;
        }

        if (interrupcionSolicitada)
        {
            abortarProcesosActivos(lista_actividades);
            return false;

        }

        if (procesos_activos == 0)
        {
            if(tareas_resueltas == total_tareas)
            {
                break;
            }

            cerr << "Error: no hay actividades disponibles para ejecutar." << endl;
            return false;
        }

        int estado;
        pid_t pidTerminado = waitpid(-1, &estado, 0);

        if(pidTerminado == -1)
        {
            if(interrupcionSolicitada)
            {
                abortarProcesosActivos(lista_actividades);
                return false;
            }

            cerr << "Error al esperar un proceso hijo." << endl;
            return false;
        }

        procesos_activos--;

        auto encontrado = posicionPorPid.find(pidTerminado);

        if(encontrado == posicionPorPid.end())
        {
            cerr << "Error: no se encontro la actividad del proceso " << pidTerminado << endl;
            return false;
        }
        int posicion = encontrado->second;
        posicionPorPid.erase(encontrado);
        Actividad& actividad = lista_actividades[posicion];

        if(WIFEXITED(estado) && WEXITSTATUS(estado) == EXIT_SUCCESS)
        {
            string mensaje = recibirInsumo(actividad);

            if(mensaje.empty())
            {
                cerr << "Error: no se recibio el mensaje de la actividad " << actividad.id_Actividad << endl;
                return false;
            }

            cout << "[PIPE] Actividad " << actividad.id_Actividad << " - mensaje recibido: " << mensaje << endl;

            if (!guardarInsumoEnDependientes(actividad, mensaje, lista_actividades, dependientes[posicion]))
            {
                return false;
            }
            actividad.completada = true;
            tareas_resueltas++;

            for (int posicionDependiente : dependientes[posicion])
            {
                Actividad& actividadDependiente = lista_actividades[posicionDependiente];

                if (actividadDependiente.bloqueada || actividadDependiente.fallida || actividadDependiente.completada)
                {
                    continue;
                }

                dependenciasPendientes[posicionDependiente]--;

                if (dependenciasPendientes[posicionDependiente] == 0)
                {
                    actividadesDisponibles.push(posicionDependiente);
                }
            }
            cout << "[PADRE] Actividad " << actividad.id_Actividad << " - completada" << endl << endl;
        }
        else 
        {
            actividad.fallida = true;
            tareas_resueltas++;

            cerr << "[ERROR] Actividad " << actividad.id_Actividad << " - fallo durante la ejecucion" <<endl;

            if (actividad.pipe_fd[0] != -1)
            {
                close(actividad.pipe_fd[0]);
                actividad.pipe_fd[0] = -1;
            }
            
            int cantidadBloqueadas = bloquearDependientes(posicion, lista_actividades, dependientes);
            tareas_resueltas += cantidadBloqueadas;

            cout << "[PLANIFICADOR] Rama de la actividad " << actividad.id_Actividad << " - cancelada" << endl << endl;
        }
    }
    return true;
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

   int k;
   size_t posicion;

    try
    {
    k = stoi(argv[2], &posicion);

     if (posicion != string(argv[2]).size())
     {
        cerr << "Error: K debe ser un numero entero." << endl;
        return 1;
     }
    }
    catch (...)
    {
    cerr << "Error: K debe ser un numero entero." << endl;
    return 1;
    } 

   if (k <= 0)
    {
    cerr << "Error: K debe ser mayor que 0." << endl;
    return 1;
    }

    cout << "Archivo que se va a leer: " << nombreArchivo << endl;
    cout << "Con el límite de concurrencia: " << k << endl;

    vector<Actividad> lista_actividades;

    if (!cargarPlan(nombreArchivo, lista_actividades))
    {
     return 1;
    }

    if (!validarIdsUnicos(lista_actividades))
    {
     return 1;
    }

    if (!validarDependencias(lista_actividades))
    {
     return 1;
    }

    if (!validarSinCiclos(lista_actividades))
    {
     return 1;
    }

    cout << endl;
    cout << "Actividades cargadas: "
         << lista_actividades.size() << endl;

    for (const Actividad& actividad : lista_actividades)
    {
        cout << endl;
        cout << "ID: " << actividad.id_Actividad << endl;
        cout << "Nombre: " << actividad.nombre_Actividad << endl;
        cout << "Tiempo: " << actividad.tiempo << " ms" << endl;

        cout << "Dependencias: ";

        if (actividad.dependencias.empty())
        {
            cout << "ninguna";
        }
        else
        {
            for (const string& dependencia : actividad.dependencias)
            {
                cout << dependencia << " ";
            }
        }

        cout << endl;
    }
   cout << endl;
   cout << "[Planificador] Iniciando ejecucion del plan..." << endl;

    if (!ejecutarPlan(lista_actividades, k))
    {
    cerrarPipesEntrada(lista_actividades);
    return 1;
    }

    int completadas = 0;
int fallidas = 0;
int bloqueadas = 0;

for (const Actividad& actividad : lista_actividades)
{
    if (actividad.completada)
    {
        completadas++;
    }
    else if (actividad.fallida)
    {
        fallidas++;
    }
    else if (actividad.bloqueada)
    {
        bloqueadas++;
    }
}

cout << "========================================" << endl;

if (fallidas == 0 && bloqueadas == 0)
{
    cout << "[PLANIFICADOR] Todas las actividades fueron completadas."
         << endl;
}
else
{
    cout << "[PLANIFICADOR] Ejecucion del plan finalizada." << endl;
    cout << "Completadas: " << completadas << endl;
    cout << "Fallidas: " << fallidas << endl;
    cout << "Bloqueadas: " << bloqueadas << endl;
}

cerrarPipesEntrada(lista_actividades);

return 0;
}