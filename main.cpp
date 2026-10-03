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

using namespace std;
struct Actividad
{
    string id_Actividad;
    string nombre_Actividad;
    int tiempo;
    vector<string> dependencias;
    vector<string> insumos;
    int pipe_fd[2];
    pid_t pid_hijo = -1;
    bool completada = false;
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

    pid_t pid = fork();

    if (pid < 0)
    {
        cerr << "Error: no se pudo crear el proceso para la actividad "
             << actividad.id_Actividad << endl;

        close(actividad.pipe_fd[0]);
        close(actividad.pipe_fd[1]);

        actividad.pipe_fd[0] = -1;
        actividad.pipe_fd[1] = -1;

        return -1;
    }

    if (pid == 0)
    {
        // El hijo solamente escribe en el pipe.
        close(actividad.pipe_fd[0]);

        cout << "[Hijo] Iniciando actividad "
             << actividad.id_Actividad
             << " - "
             << actividad.nombre_Actividad
             << endl;

        usleep(actividad.tiempo * 1000);

        cout << "[Hijo] Actividad "
             << actividad.id_Actividad
             << " terminada."
             << endl;

        if (!propagarMensaje(actividad))
        {
            close(actividad.pipe_fd[1]);
            _exit(EXIT_FAILURE);
        }

        close(actividad.pipe_fd[1]);

        _exit(EXIT_SUCCESS);
    }

    // El padre solamente lee del pipe.
    close(actividad.pipe_fd[1]);
    actividad.pipe_fd[1] = -1;

    actividad.pid_hijo = pid;

    cout << "[Padre] Se creo el proceso "
         << pid
         << " para la actividad "
         << actividad.id_Actividad
         << endl;

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

void guardarInsumoEnDependientes(
    const Actividad& actividad_finalizada,
    const string& mensaje,
    vector<Actividad>& lista_actividades)
{
    for (Actividad& actividad : lista_actividades)
    {
        for (const string& dependencia : actividad.dependencias)
        {
            if (dependencia == actividad_finalizada.id_Actividad)
            {
                actividad.insumos.push_back(mensaje);

                cout << "[PIPE] Insumo de la actividad "
                     << actividad_finalizada.id_Actividad
                     << " preparado para la actividad "
                     << actividad.id_Actividad
                     << endl;

                break;
            }
        }
    }
}

bool ejecutarPlan(vector<Actividad>& lista_actividades, int k)
{
    int procesos_activos = 0;
    int tareas_completadas = 0;
    int total_tareas = static_cast<int>(lista_actividades.size());

    while (tareas_completadas < total_tareas)
    {
        for (Actividad& actividad : lista_actividades)
        {
            if (procesos_activos >= k)
            {
                break;
            }

            bool noIniciada = (actividad.pid_hijo == -1);

            if (noIniciada &&
                !actividad.completada &&
                dependenciasCompletadas(actividad, lista_actividades))
            {
                pid_t pid = crearProcesoActividad(actividad);

                if (pid == -1)
                {
                    cerr << "Error al crear un proceso." << endl;
                    return false;
                }

                procesos_activos++;
            }
        }

        if (procesos_activos == 0)
        {
            cerr << "Error: no hay actividades disponibles para ejecutar."
                 << endl;

            return false;
        }

        int estado;

        pid_t pidTerminado = waitpid(-1, &estado, 0);

        if (pidTerminado == -1)
        {
            cerr << "Error al esperar un proceso hijo." << endl;
            return false;
        }

        procesos_activos--;

        int posicion =
            buscarActividadPorPid(lista_actividades, pidTerminado);

        if (posicion == -1)
        {
            cerr << "Error: no se encontro la actividad del proceso "
                 << pidTerminado << endl;

            return false;
        }
        if (WIFEXITED(estado) &&
          WEXITSTATUS(estado) == EXIT_SUCCESS)
        {
        string mensaje =
        recibirInsumo(lista_actividades[posicion]);

        if (mensaje.empty())
        {
            cerr << "Error: no se recibio el mensaje de la actividad "
                 << lista_actividades[posicion].id_Actividad
                 << endl;

             return false;
       }

           cout << "[PIPE] Mensaje recibido: "
                << mensaje
                << endl;

        guardarInsumoEnDependientes(
              lista_actividades[posicion],
              mensaje,
              lista_actividades);

         lista_actividades[posicion].completada = true;
         tareas_completadas++;

           cout << "[Padre] Actividad "
                << lista_actividades[posicion].id_Actividad
                << " completada."
                << endl;
}
         else
         {
            cerr << "[Padre] La actividad "
                 << lista_actividades[posicion].id_Actividad
                 << " termino con error."
                 << endl;

            return false;
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

   try
    {
    k = stoi(argv[2]);
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
    return 1;
    }

    cout << endl;
    cout << "[Planificador] Todas las actividades fueron completadas."
     << endl;

     return 0;
}