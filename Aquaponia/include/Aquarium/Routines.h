#ifndef ROUTINES_H
#define ROUTINES_H

struct horario{
    ushort start;
    ushort end;
};

struct routine{
    bool weekday[7];
    vector<horario> horarios;
};

#endif