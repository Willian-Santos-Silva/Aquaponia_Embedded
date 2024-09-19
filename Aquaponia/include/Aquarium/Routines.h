#ifndef ROUTINES_H
#define ROUTINES_H

#include "uuid.h"
#include <vector>

using namespace std;

struct horario{
    ushort start = 0;
    ushort end = 0;
};

struct routine{
    char id[37];
    bool weekday[7];
    // horario* horarios;
    vector<horario> horarios;
    // routine() : horarios(new horario[720]){} // Construtor para alocar memória
    // ~routine() { delete[] horarios; }
};

#endif