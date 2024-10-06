#ifndef ROUTINES_H
#define ROUTINES_H

#include <vector>

using namespace std;

struct horario{
    ushort start = 1441;
    ushort end = 1441;
};

struct routine{
    char id[37];
    bool weekday[7];
    horario horarios[720];

    routine() : id("") {} // Construtor para alocar memória
    ~routine() {   }
};

#endif