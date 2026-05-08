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
int32_t collect_LV_Battery_mV(bool* cancelFrameSend){
    int32_t LV_Battery_mV = 0;
	mutexPrint("collecting LV_Battery_mV\n");
    return LV_Battery_mV;
}

int32_t collect_LV_Battery_mA(bool* cancelFrameSend){
    int32_t LV_Battery_mA = 0;
	mutexPrint("collecting LV_Battery_mA\n");
    return LV_Battery_mA;
}

int32_t collect_CoolantPumpAvgCurrent_mA(bool* cancelFrameSend){
    int32_t CoolantPumpAvgCurrent_mA = 0;
	mutexPrint("collecting CoolantPumpAvgCurrent_mA\n");
    return CoolantPumpAvgCurrent_mA;
}

int32_t collect_CoolantPumpPeakCurrent_mA(bool* cancelFrameSend){
    int32_t CoolantPumpPeakCurrent_mA = 0;
	mutexPrint("collecting CoolantPumpPeakCurrent_mA\n");
    return CoolantPumpPeakCurrent_mA;
}

int32_t collect_CoolantPump_Freq_kHz(bool* cancelFrameSend){
    int32_t CoolantPump_Freq_kHz = 0;
	mutexPrint("collecting CoolantPump_Freq_kHz\n");
    return CoolantPump_Freq_kHz;
}

int32_t collect_CoolantPumpDutyCycle(bool* cancelFrameSend){
    int32_t CoolantPumpDutyCycle = 0;
	mutexPrint("collecting CoolantPumpDutyCycle\n");
    return CoolantPumpDutyCycle;
}

int32_t collect_CoolantDriver_Fault(bool* cancelFrameSend){
    int32_t CoolantDriver_Fault = 0;
	mutexPrint("collecting CoolantDriver_Fault\n");
    return CoolantDriver_Fault;
}

void receiveMSG(){  //task handles recieving Messages
	PCANListenParamsCollection plpc={ .arr={{0}}, .defaultHandler = defaultPacketRecv, .size = 0, };
	sensorInit(&plpc,NULL); //vitals Compliance

	//declare CanListenparams here, each param has 3 entries:
	//When recv msg with id = 'listen_id' according to matchtype (or 'mt'), 'handler' is called.
	
//task calls the appropriate ListenParams function when a CAN message is recieved
	for(;;){
		while(waitPackets(&plpc) != NOT_RECEIVED);
		taskYIELD();
	}
}

void app_main(void){
	base_ESP_init();
	pecanInit config={.nodeId= myId, .pin1= defaultPin, .pin2= defaultPin};
	pecan_CanInit(config);   //initialize CAN

	//Declare tasks here as needed
	TaskHandle_t recieveHandler = xTaskCreateStaticPinnedToCore(  //recieves CAN Messages 
		receiveMSG,       /* Function that implements the task. */
		"msgRecieve",          /* Text name for the task. */
		STACK_SIZE,      /* Number of indexes in the xStack array. */
		( void * ) 1,    /* Task Parameter. Must remain in scope or be constant!*/ 
		tskIDLE_PRIORITY,/* Priority at which the task is created. */
		receiveMSG_Stack,          /* Array to use as the task's stack. */
		&receiveMSG_Buffer,   /* Variable to hold the task's data structure. */
		tskNO_AFFINITY);  //assign to either core
}void receiveMSG(){  //task handles recieving Messages
	PCANListenParamsCollection plpc={ .arr={{0}}, .defaultHandler = defaultPacketRecv, .size = 0, };
	sensorInit(&plpc,NULL); //vitals Compliance

	//declare CanListenparams here, each param has 3 entries:
	//When recv msg with id = 'listen_id' according to matchtype (or 'mt'), 'handler' is called.
	
//task calls the appropriate ListenParams function when a CAN message is recieved
	for(;;){
		while(waitPackets(&plpc) != NOT_RECEIVED);
		taskYIELD();
	}
}

void app_main(void){
	base_ESP_init();
	pecanInit config={.nodeId= myId, .pin1= defaultPin, .pin2= defaultPin};
	pecan_CanInit(config);   //initialize CAN

	//Declare tasks here as needed
	TaskHandle_t recieveHandler = xTaskCreateStaticPinnedToCore(  //recieves CAN Messages 
		receiveMSG,       /* Function that implements the task. */
		"msgRecieve",          /* Text name for the task. */
		STACK_SIZE,      /* Number of indexes in the xStack array. */
		( void * ) 1,    /* Task Parameter. Must remain in scope or be constant!*/ 
		tskIDLE_PRIORITY,/* Priority at which the task is created. */
		receiveMSG_Stack,          /* Array to use as the task's stack. */
		&receiveMSG_Buffer,   /* Variable to hold the task's data structure. */
		tskNO_AFFINITY);  //assign to either core
}