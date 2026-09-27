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

    return 0;
}