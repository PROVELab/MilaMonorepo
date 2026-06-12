#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "driver/gpio.h"
#include "driver/twai.h"
#include "freertos/semphr.h"
#include <string.h>
#include "esp_timer.h"

#include "../../pecan/pecan.h"             //used for CAN
#include "../common/sensorHelper.hpp"      //used for compliance with vitals and sending data
#include "myDefines.hpp"       //contains #define statements specific to this node like myId.
#include "../../espBase/debug_esp.h"
//add declerations to allocate space for additional tasks here as needed
StaticTask_t receiveMSG_Buffer;
StackType_t receiveMSG_Stack[STACK_SIZE]; //buffer that the task will use as its stack

//For Standard behavior, fill in the collectData<NAME>() function(s).
//In the function, return an int32_t with the corresponding data
