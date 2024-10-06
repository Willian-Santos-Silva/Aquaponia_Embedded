#ifndef TASK
#define TASK

#include <functional>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "soc/rtc_wdt.h"

class TaskWrapper
{
public:
    TaskWrapper()
    {
        semaphoreTask = xSemaphoreCreateBinary();
        
    }

    void begin(const std::function<void()> &taskFunction, const char *taskName, uint32_t stackSize, UBaseType_t priority) 
    {
        if(semaphoreTask == NULL){
           log_e("semaphoreTask NULL");
           return;
        }
        xSemaphoreGive(semaphoreTask);

        _taskFunction = taskFunction;
        xTaskCreate(&TaskWrapper::taskEntryPoint, taskName, stackSize, this, priority, &_handleTask);
        Serial.printf("Tamanho task: %i\r\n",uxTaskGetStackHighWaterMark(_handleTask));
    }
    void beginMainCore(const std::function<void()> &taskFunction, const char *taskName, uint32_t stackSize, UBaseType_t priority) 
    {
        if(semaphoreTask == NULL){
           log_e("semaphoreTask NULL");
           return;
        }
         xSemaphoreGive(semaphoreTask);

        _taskFunction = taskFunction;
        xTaskCreatePinnedToCore(&TaskWrapper::taskEntryPoint, taskName, stackSize, this, priority, &_handleTask, APP_CPU_NUM);
    }
    void beginSecondaryCore(const std::function<void()> &taskFunction, const char *taskName, uint32_t stackSize, UBaseType_t priority) 
    {
        if(semaphoreTask == NULL){
           log_e("semaphoreTask NULL");
           return;
        }
         xSemaphoreGive(semaphoreTask);

        _taskFunction = taskFunction;
        xTaskCreatePinnedToCore(&TaskWrapper::taskEntryPoint, taskName, stackSize, this, priority, &_handleTask, PRO_CPU_NUM);
    }


    void pause()
    {
        vTaskSuspend(_handleTask);
    }

    void resume()
    {
        vTaskResume(_handleTask);
    }
    void awaitTask(TaskWrapper task)
    {
        xSemaphoreTake(task.getTask(), portMAX_DELAY);
    }
    void finishTask()
    {
        xSemaphoreGive(semaphoreTask);
    }
    SemaphoreHandle_t getTask()
    {
        return semaphoreTask;
    }

private:
    std::function<void()> _taskFunction;
    SemaphoreHandle_t semaphoreTask;
    TaskHandle_t _handleTask = NULL;

    static void taskEntryPoint(void *parameter)
    {
        TaskWrapper *taskWrapper = static_cast<TaskWrapper *>(parameter);
        if (taskWrapper != nullptr)
        {
            taskWrapper->_taskFunction();
        }
    }
};
#endif