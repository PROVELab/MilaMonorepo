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
int32_t collect_IMU_temp_F(bool* cancelFrameSend){
    int32_t IMU_temp_F = 67;
	mutexPrint("collecting IMU_temp_F\n");
    return IMU_temp_F;
}

int32_t collect_radiator_temp_F(bool* cancelFrameSend){
    int32_t radiator_temp_F = 67;
	mutexPrint("collecting radiator_temp_F\n");
    return radiator_temp_F;
}

int32_t collect_humiditySense_temp_F(bool* cancelFrameSend){
    int32_t humiditySense_temp_F = 67;
	mutexPrint("collecting humiditySense_temp_F\n");
    return humiditySense_temp_F;
}

int32_t collect_RH(bool* cancelFrameSend){
    int32_t RH = 1;
	mutexPrint("collecting RH\n");
    return RH;
}

int32_t collect_posX_m(bool* cancelFrameSend){
    int32_t posX_m = 0;
	mutexPrint("collecting posX_m\n");
    return posX_m;
}

int32_t collect_posY_m(bool* cancelFrameSend){
    int32_t posY_m = 0;
	mutexPrint("collecting posY_m\n");
    return posY_m;
}

int32_t collect_posZ_m(bool* cancelFrameSend){
    int32_t posZ_m = 0;
	mutexPrint("collecting posZ_m\n");
    return posZ_m;
}

int32_t collect_accelY_mm_p_ss(bool* cancelFrameSend){
    int32_t accelY_mm_p_ss = 0;
	mutexPrint("collecting accelY_mm_p_ss\n");
    return accelY_mm_p_ss;
}

int32_t collect_accelZ_mm_p_ss(bool* cancelFrameSend){
    int32_t accelZ_mm_p_ss = 0;
	mutexPrint("collecting accelZ_mm_p_ss\n");
    return accelZ_mm_p_ss;
}

int32_t collect_gyroX_deg_p_s(bool* cancelFrameSend){
    int32_t gyroX_deg_p_s = 0;
	mutexPrint("collecting gyroX_deg_p_s\n");
    return gyroX_deg_p_s;
}

int32_t collect_gryoY_deg_p_s(bool* cancelFrameSend){
    int32_t gryoY_deg_p_s = 0;
	mutexPrint("collecting gryoY_deg_p_s\n");
    return gryoY_deg_p_s;
}

int32_t collect_gyroZ_deg_p_s(bool* cancelFrameSend){
    int32_t gyroZ_deg_p_s = 0;
	mutexPrint("collecting gyroZ_deg_p_s\n");
    return gyroZ_deg_p_s;
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