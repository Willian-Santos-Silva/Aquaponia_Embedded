#ifndef CLOCK_H
#define CLOCK_H

#include "ErriezDS1302.h"

#include <time.h>
#include "Clock/Date.h"
#include "Base/config.h"

class Clock
{
private:
    ErriezDS1302 rtc;
    Date now;

public:
    Clock() : rtc(PIN_CLOCK_CLK, PIN_CLOCK_DAT, PIN_CLOCK_RST)
    {
        log_e("Iniciando RTC");
        while(!rtc.begin()){
            delay(200);
        }
    }

    void setTime(int timezone){
      struct tm timeinfo;
      configTime(timezone * 3600, 0, "pool.ntp.org");

      if(!getLocalTime(&timeinfo, 50000U)){
        throw std::runtime_error("Falha ao obter horario");
      }

    //   log_e("%A, %B %d %Y %H:%M:%S zone %Z %z ", &timeinfo);
      setRTC(&timeinfo);

    //   log_e("%s", getDateTimeString());
    }

    void setRTC(tm *date)
    {
        // Serial.printf("%A, %B %d %Y %H:%M:%S zone %Z %z \r\n", date);
        if (!isRunningClock())
            rtc.clockEnable(true);

        if(!rtc.setDateTime(date->tm_hour, date->tm_min, date->tm_sec, date->tm_mday, date->tm_mon + 1, date->tm_year + 1900, date->tm_wday)){
            throw std::runtime_error("Erro ao definir data e hora");
        }

        log_e("Sucesso ao definir horario");
    }

    bool isRunningClock()
    {
        return rtc.isRunning();
    }
    tm getDateTime()
    {   
        time_t t = rtc.getEpoch();
        // if(!rtc.getDateTime(&now.hour, &now.minute, &now.second, &now.day, &now.month, &now.year, &now.day_of_week)){
        //     throw std::runtime_error("Falha ao ler RTC");
        // }

        return *gmtime(&t);
    }
    const char * getDateTimeString()
    {
        time_t t = rtc.getEpoch();
        tm *timeinfo = gmtime(&t);

        static char dateTimeStr[30];
        snprintf(dateTimeStr, sizeof(dateTimeStr), "%02d/%02d/%04d %02d:%02d:%02d",
             timeinfo->tm_mday, timeinfo->tm_mon + 1, timeinfo->tm_year + 1900,
             timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec);
    
        free(timeinfo);
        return dateTimeStr;
                    
    }
    time_t getTimestamp()
    {
        return rtc.getEpoch();
                    
    }
};

#endif