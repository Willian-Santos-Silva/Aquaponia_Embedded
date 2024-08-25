#ifndef DATE_H
#define DATE_H
#include <Arduino.h>
#include <stdint.h>
#include <time.h>

struct Date
{
    uint16_t year;
    uint8_t month;
    uint8_t day;
    uint8_t hour;
    uint8_t minute;
    uint8_t second;
    uint8_t day_of_week;
    
    const char* getFullDate(){
        return (String(year) + "/" + String(month) + "/" + String(day) + " " +
               String(hour) + ":" + String(minute) + ":" + String(second)).c_str();
    }
};

#endif