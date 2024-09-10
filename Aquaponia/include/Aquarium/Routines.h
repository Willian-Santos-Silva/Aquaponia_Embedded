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
    vector<horario> horarios;
};

struct aplicacoes{
    double ml;    
    time_t dataAplicacao;    
    bool type;
    double deltaPh;
};
#endif