#ifndef CLOCK_H
#define CLOCK_H

#include <ErriezDS1302.h>

#include "Clock/Date.h"
#include "config.h"

class Clock
{
private:
    ErriezDS1302 rtc;
    Date now;

public:
    Clock() : rtc(PIN_CLOCK_CLK, PIN_CLOCK_DAT,PIN_CLOCK_RST)
    {
        Serial.print("Iniciando RTC");
        while(!rtc.begin()){
            Serial.print(".");
            delay(200);
        }
        Serial.print("\r\n");
    }

    void setClock(Date date)
    {
        if (!isRunningClock())
            rtc.clockEnable(true);


        if(!rtc.setDateTime(date.hour, date.minute, date.second, date.day, date.month, date.year, date.day_of_week)){
            Serial.println("Erro ao definir data e hora");
            return;
        }

        Serial.println("Sucesso ao definir horario");
    }

    void setAlarm(Date date)
    {
    }

    bool isRunningClock()
    {
        return rtc.isRunning();
    }
    Date getDateTime()
    {
        rtc.getDateTime(&now.hour, &now.minute, &now.second, &now.day, &now.month, &now.year, &now.day_of_week);
        return now;
    }
};

#endif