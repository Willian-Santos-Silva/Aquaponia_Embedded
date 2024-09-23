#ifndef APLLICACOES_H
#define APLLICACOES_H

#include <time.h>

using namespace std;

struct aplicacoes {
    double ml = 0.0;    
    time_t dataAplicacao;    
    int type;
    double deltaPh = 0.0;
};

#endif