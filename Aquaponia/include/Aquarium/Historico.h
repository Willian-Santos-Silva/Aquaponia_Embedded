#ifndef HISTORICO_H
#define HISTORICO_H

#include <time.h>

using namespace std;

struct historicoPh {
    double ph;    
    time_t data;
};

struct historicoTemperatura {
    double temperatura;    
    time_t data;
};

#endif