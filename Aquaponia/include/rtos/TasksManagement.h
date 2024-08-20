#ifndef TASK
#define TASK

#include <functional>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

class TaskWrapper
{
public:
    TaskWrapper()
    {
        _handleTask = xSemaphoreCreateBinary();
        if(_handleTask == NULL){
           Serial.print("_handleNulo");
        }
    }

    void begin(const std::function<void()> &taskFunction, const char *taskName, uint32_t stackSize, UBaseType_t priority) 
    {
        _taskFunction = taskFunction;
        xTaskCreate(&TaskWrapper::taskEntryPoint, taskName, stackSize, this, priority, NULL);
    }

    void awaitTask(TaskWrapper task)
    {
        xSemaphoreTake(task.getTask(), portMAX_DELAY);
    }
    void finishTask()
    {
        xSemaphoreGive(_handleTask);
    }
    SemaphoreHandle_t getTask()
    {
        return _handleTask;
    }

private:
    std::function<void()> _taskFunction;
    SemaphoreHandle_t _handleTask;

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