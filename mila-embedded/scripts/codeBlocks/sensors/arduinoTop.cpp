#include <Arduino.h>
#include <avr/wdt.h>
#include "CAN.h"
#include "../../pecan/pecan.h"                  //used for CAN
#include "../../arduinoSched/arduinoSched.hpp"  //used for scheduling
#include "../common/sensorHelper.hpp"      //used for compliance with vitals and sending data
#include "myDefines.hpp"    //contains #define statements specific to this node like myId.

PCANListenParamsCollection plpc={ .arr={{0}}, .defaultHandler = defaultPacketRecv, .size = 0};
PScheduler ts;
//For Standard behavior, fill in the collectData<NAME>() function(s).
//In the function, return an int32_t with the corresponding data
