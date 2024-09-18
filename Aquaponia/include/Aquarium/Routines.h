#ifndef ROUTINES_H
#define ROUTINES_H

#include "uuid.h"
#include <vector>

using namespace std;

struct horario{
    ushort start;
    ushort end;
};

struct routine{
    char id[37];
    bool weekday[7];
    horario* horarios;

    routine() : horarios(new horario[1440]){} // Construtor para alocar memória
    ~routine() { delete[] horarios; }
};

#endif