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
#include "helper/myDefines.hpp"       //contains #define statements specific to this node like myId.
#include "../../espBase/debug_esp.h"
#include "esp_log.h"

#include "imu_manager.h"

static const char* TAG = "SensorMain";
//add declerations to allocate space for additional tasks here as needed
StaticTask_t receiveMSG_Buffer;
StackType_t receiveMSG_Stack[STACK_SIZE]; //buffer that the task will use as its stack

//For Standard behavior, fill in the collectData<NAME>() function(s).
//In the function, return an int32_t with the corresponding data
#include "imu_manager.h" 
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// --- FRAME 1: Temperature & Humidity (100Hz) ---

int32_t collect_IMU_temp_F(bool* cancelFrameSend){
    // Top function: Refresh the IMU cache here.
    if(!imu_refreshData()){
        ESP_LOGI(TAG, "error refreshing imu data. is it initialized? Cancelling data collection");
        *cancelFrameSend = true;
        return 67; // Return starting value on failure
    }

    // Convert Celsius to Fahrenheit
    float temp_C = read_temperature();
    int32_t IMU_temp_F = (int32_t)((temp_C * 9.0f / 5.0f) + 32.0f);
    
    // ESP_LOGI(TAG, "collecting IMU_temp_F");
    return IMU_temp_F;
}

int32_t collect_radiator_temp_F(bool* cancelFrameSend){
    int32_t radiator_temp_F = 67; // Starting value
    return radiator_temp_F;
}

int32_t collect_humiditySense_temp_F(bool* cancelFrameSend){
    int32_t humiditySense_temp_F = 67; // Starting value
    return humiditySense_temp_F;
}

int32_t collect_RH(bool* cancelFrameSend){
    int32_t RH = 1; // Starting value
    return RH;
}


// --- FRAME 2: Position  ---
int32_t collect_posX_m(bool* cancelFrameSend){
	if(!imu_refreshData()){
        ESP_LOGI(TAG, "error refreshing imu data. is it initialized? Cancelling data collection");
        *cancelFrameSend = true;    
        return -1;
    }

    imu_vector3_t pos = read_position();
    int32_t posX_m = (int32_t)pos.x;
    return posX_m;
}

int32_t collect_posY_m(bool* cancelFrameSend){
    imu_vector3_t pos = read_position();
    int32_t posY_m = (int32_t)pos.y;
    return posY_m;
}

int32_t collect_posZ_m(bool* cancelFrameSend){
    imu_vector3_t pos = read_position();
    int32_t posZ_m = (int32_t)pos.z;
    return posZ_m;
}


// --- FRAME 3: Acceleration---

int32_t collect_accelX_miliGs(bool* cancelFrameSend){
	if(!imu_refreshData()){
        ESP_LOGI(TAG, "error refreshing imu data. is it initialized? Cancelling data collection");
        *cancelFrameSend = true;
        return -1; 
    }
    // Native output is 'g's. Multiply by 1000 for milli-g.
    imu_vector3_t accel = read_accelerometer();
    int32_t accelX_miliGs = (int32_t)(accel.x * 1000.0f);
    return accelX_miliGs;
}

int32_t collect_accelY_miliGs(bool* cancelFrameSend){
    imu_vector3_t accel = read_accelerometer();
    int32_t accelY_miliGs = (int32_t)(accel.y * 1000.0f);
    return accelY_miliGs;
}

int32_t collect_accelZ_miliGs(bool* cancelFrameSend){
    imu_vector3_t accel = read_accelerometer();
    int32_t accelZ_miliGs = (int32_t)(accel.z * 1000.0f);
    return accelZ_miliGs;
}

//Frame 5: Orientation
int32_t collect_yaw_degrees(bool* cancelFrameSend){
	if(!imu_refreshData()){
        ESP_LOGI(TAG, "error refreshing imu data. is it initialized? Cancelling data collection");
        *cancelFrameSend = true;
        return -1; // Return starting value on failure
    }
    // Native output is radians. Convert to standard degrees.
    // Range is naturally -180 to 180 (from -pi to pi).
    imu_orientation_t ori = read_madgwick_orientation();
    int32_t yaw_degrees = (int32_t)(ori.yaw * (180.0f / M_PI));
    return yaw_degrees;
}

int32_t collect_pitch_degrees(bool* cancelFrameSend){
    imu_orientation_t ori = read_madgwick_orientation();
    int32_t pitch_degrees = (int32_t)(ori.pitch * (180.0f / M_PI));
    return pitch_degrees;
}

int32_t collect_roll_degrees(bool* cancelFrameSend){
    imu_orientation_t ori = read_madgwick_orientation();
    int32_t roll_degrees = (int32_t)(ori.roll * (180.0f / M_PI));
    return roll_degrees;
}

int32_t collect_gyroX_deciDegree_p_s(bool* cancelFrameSend){
    // Native output is degrees per second. Multiply by 10 for deci-degrees.
    imu_vector3_t gyro = read_gyroscope();
    int32_t gyroX_deciDegree_p_s = (int32_t)(gyro.x * 10.0f);
    return gyroX_deciDegree_p_s;
}

int32_t collect_gyroY_deciDegree_p_s(bool* cancelFrameSend){
    // Kept the spelling 'gryo' to exactly match your parameter name
    imu_vector3_t gyro = read_gyroscope();
    int32_t gryoY_deciDegree_p_s = (int32_t)(gyro.y * 10.0f);
    return gryoY_deciDegree_p_s;
}

int32_t collect_gyroZ_deciDegree_p_s(bool* cancelFrameSend){
    imu_vector3_t gyro = read_gyroscope();
    int32_t gyroZ_deciDegree_p_s = (int32_t)(gyro.z * 10.0f);
    return gyroZ_deciDegree_p_s;
}

void receiveMSG(void* pvParameters){  //task handles recieving Messages
	PCANListenParamsCollection plpc={ .arr={{0}}, .defaultHandler = defaultPacketRecv, .size = 0, };
	sensorInit(&plpc,NULL); //vitals Compliance
#ifdef SENSOR_HAS_COMMANDS
	registerCommandHandler(&plpc);
#endif

	//declare CanListenparams here, each param has 3 entries:
	//When recv msg with id = 'listen_id' according to matchtype (or 'mt'), 'handler' is called.
	
//task calls the appropriate ListenParams function when a CAN message is received
	for(;;){
		while(waitPackets(&plpc) != NOT_RECEIVED);
		taskYIELD();
	}
}

void app_main(void){
	base_ESP_init();
	pecanInit config={.nodeId= myId, .pin1= defaultPin, .pin2= defaultPin};
	imu_init();
	pecan_CanInit(config);   //initialize CAN

	//Declare tasks here as needed
	TaskHandle_t receiveHandler = xTaskCreateStaticPinnedToCore(  //receives CAN Messages 
		receiveMSG,       /* Function that implements the task. */
		"msgreceive",          /* Text name for the task. */
		STACK_SIZE,      /* Number of indexes in the xStack array. */
		( void * ) 1,    /* Task Parameter. Must remain in scope or be constant!*/ 
		tskIDLE_PRIORITY,/* Priority at which the task is created. */
		receiveMSG_Stack,          /* Array to use as the task's stack. */
		&receiveMSG_Buffer,   /* Variable to hold the task's data structure. */
		tskNO_AFFINITY);  //assign to either core
}
