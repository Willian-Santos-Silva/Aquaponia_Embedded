#ifndef CLOCK_H
#define CLOCK_H

#include <ErriezDS1302.h>

#include <time.h>
#include "Clock/Date.h"
#include "config.h"

class Clock
{
private:
    ErriezDS1302 rtc;
    Date now;

public:
    Clock() : rtc(PIN_CLOCK_CLK, PIN_CLOCK_DAT, PIN_CLOCK_RST)
    {
        Serial.print("Iniciando RTC");
        while(!rtc.begin()){
            Serial.print(".");
            delay(200);
        }
        Serial.print("\r\n");
    }

    void setTime(int timezone){
      struct tm timeinfo;
      configTime(timezone * 3600, 0, "pool.ntp.org");
      if(!getLocalTime(&timeinfo, 50000U)){
        Serial.println("Falha ao obter horario");
        return;
      }
      Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S zone %Z %z ");
      setClock(timeinfo);

      Serial.println(getDateTime().getFullDate());
    }


    void setClock(tm date)
    {
        if (!isRunningClock())
            rtc.clockEnable(true);


        if(!rtc.setDateTime(date.tm_hour, date.tm_min, date.tm_sec, date.tm_mday, date.tm_mon + 1, date.tm_year + 1900, date.tm_wday)){
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
        if(!rtc.getDateTime(&now.hour, &now.minute, &now.second, &now.day, &now.month, &now.year, &now.day_of_week)){
            // Serial.println("Falha ao ler RTC");
            return Date();
        }
        return now;
    }
};

#endif