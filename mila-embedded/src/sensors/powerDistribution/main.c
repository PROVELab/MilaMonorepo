#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "driver/gpio.h"
#include "driver/twai.h"
#include "freertos/semphr.h"
#include <string.h>
#include "esp_timer.h"

#include "../../pecan/pecan.h"             
#include "../common/sensorHelper.hpp"      
#include "helper/myDefines.hpp"
#include "../../espBase/debug_esp.h"
#include "esp_log.h"

// --- Include the new Coolant Subsystem ---
#include "coolant.h"
#include "intermoduleContactor.h"

static const char* TAG = "SensorMain";
StaticTask_t receiveMSG_Buffer;
StackType_t receiveMSG_Stack[STACK_SIZE]; 

// --- Sensor Collection Functions ---
int32_t collect_LV_Battery_mV(bool* cancelFrameSend){
    return 25000; // TODO: Implement if needed
}

int32_t collect_LV_Battery_mA(bool* cancelFrameSend){
    return 0; // TODO: Implement if needed
}

int32_t collect_CoolantAvgCurrent_mA(bool* cancelFrameSend){
    return get_coolant_avg_current_mA();
}

int32_t collect_CoolantPeakCurrent_mA(bool* cancelFrameSend){
    return get_coolant_peak_current_mA();
}

int32_t collect_Coolant_Freq_kHz(bool* cancelFrameSend){
    return get_coolant_freq_khz();
}

int32_t collect_CoolantDutyCycle(bool* cancelFrameSend){
	ESP_LOGI(TAG, "collect_CoolantPumpDutyCycle called");
    return get_coolant_duty_cycle();
}

int32_t collect_CoolantDriver_Fault(bool* cancelFrameSend){
    return get_coolant_fault_active();
}

void receiveMSG(void* pvParameters){  
    PCANListenParamsCollection plpc={ .arr={{0}}, .defaultHandler = defaultPacketRecv, .size = 0, };
    sensorInit(&plpc, NULL); 
    registerCommandHandler(&plpc); // Register command handlers for this sensor
    registerVitalsContactorHandler(&plpc);

    for(;;){
        // waitPackets will block until a packet is received and handled by a callback.
        // The loop ensures we immediately wait for the next packet.
        waitPackets(&plpc);
    }
}

void app_main(void){
    base_ESP_init();
    
    // Initialize CAN
    pecanInit config={.nodeId= myId, .pin1= defaultPin, .pin2= defaultPin};
    pecan_CanInit(config);   

    // Initialize the Coolant Hardware and Tasks
    init_coolant();

    TaskHandle_t receiveHandler = xTaskCreateStaticPinnedToCore(  
        receiveMSG,       
        "msgreceive",          
        STACK_SIZE,      
        ( void * ) 1,     
        5, // Use a priority higher than idle for the CAN receive task
        receiveMSG_Stack,          
        &receiveMSG_Buffer,   
        tskNO_AFFINITY);  
}
