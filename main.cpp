#include <iostream>
#include <string>
#include <vector>
#include <fstream>
#include <cstdlib>

using namespace std;

struct Actividad
{
    string id_Actividad;
    string nombre_Actividad;
    int tiempo;
    vector<string> dependencias;
};

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
    cout << "Archivo que se va a leer: " <<nombreArchivo << endl;
    cout << "Con el límite de concurrencia: " << k << endl;

    
}


