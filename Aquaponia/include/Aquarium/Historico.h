#ifndef HISTORICO_H
#define HISTORICO_H

#include <time.h>

using namespace std;

struct historicoPh {
    double ph;    
    time_t time;
};

struct historicoTemperatura {
    double temperatura;    
    time_t time;
};

#endif