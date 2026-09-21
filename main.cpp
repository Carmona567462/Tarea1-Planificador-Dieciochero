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


