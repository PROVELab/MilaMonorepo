#include <RadioLib.h>
#include "SX1262_Ext.hpp"
#include "EspHal.h"
#include <string.h>
#include <stdint.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "esp_random.h"

#include <assert.h>

#include "LoraDriver.hpp"
#include "LoraDriverConfig.hpp"

static const char *TAG = "LoraDrive";


RadioPinout pins = getRadioPins();
//RadioLib Dec
EspHal RadioLibHal = EspHal(pins.sclk, pins.miso, pins.mosi); 
Module mod = Module(&RadioLibHal, pins.nss, pins.dio1, pins.nrst, pins.busy); 
SX1262_Ext radio = SX1262_Ext(&mod);

static uint8_t rxBuffer[256]; // DMA-capable if global

#define LORA_STACK_SIZE 4096
StaticTask_t LORA_Buffer;
StackType_t LORA_Stack[LORA_STACK_SIZE];
static TaskHandle_t driverTaskHandle;

#define LORA_RX_PRIORITY 10 //should be near highest

static void DIO1_ISR();
static void loraInterruptTask(void *pvParameters);

//************** driver state and mutexes **************//

//Mutex for using driver. I dont think RadioLib is thread safe
static SemaphoreHandle_t Driver_Mutex = NULL;
static StaticSemaphore_t Driver_Mutex_Buffer;

// Semaphore for DIO1 interrupt
static SemaphoreHandle_t DIO1_Binary = NULL;
static StaticSemaphore_t DIO1_Binary_Buffer;

static bool driverStarted = false;
inline bool driverGrab(){
    while(xSemaphoreTake(Driver_Mutex, portMAX_DELAY) != pdTRUE);
    if(!driverStarted){   
        xSemaphoreGive(Driver_Mutex);
        return false;   //driver not started, should exit!
    }
    return true;   //driver is started, and we grabbed the mutex
}

inline void driverYield(){
    xSemaphoreGive(Driver_Mutex); //give mutex back
}
//************** driver state  and mutexes END **************//

//************** driver crash handling **************//
#define MAX_DRIVER_ATTEMPTS 5
#define DRIVER_RETRY_DELAY_MS 20
//give up and crash driver, driver wont do anything until driverRestart called
static void driverCrash(int16_t error, const char* msg);

//will retry until action returns RADIOLIB_ERR_NONE := 0
//return false on success, true on fail. On fail, tells driver to crash
template <typename Func>
int16_t driverCheck(Func action, const char* msg) {
    int16_t err;
    for(uint8_t attempts = 0; attempts < MAX_DRIVER_ATTEMPTS; attempts++){
        if((err = action()) == RADIOLIB_ERR_NONE){  //we passed
            return RADIOLIB_ERR_NONE;
        }
        attempts++;
        vTaskDelay(pdMS_TO_TICKS(DRIVER_RETRY_DELAY_MS));
    }
    driverCrash(err, msg);
    return err;    //action did not succeed after MAX_DRIVER_ATTEMPTS
}
//small helper for most cases. Quit early on repeated Error
#define DRIVER_CHECK(expression, msg) if (driverCheck([&]{return expression;}, msg)){ return; }

static void driverCrash(int16_t error, const char* msg){
    ESP_LOGE(TAG, "raising driver crash from %s with error %d", msg, error);
    driverStarted = false;
    protocolCrash(error, msg);

    //drop any mutexes we hold
    TaskHandle_t currTask = xTaskGetCurrentTaskHandle();
    if(currTask == driverTaskHandle){
        if(xSemaphoreGetMutexHolder( Driver_Mutex ) == currTask){
            xSemaphoreGive(Driver_Mutex);
        }
        vTaskSuspend(driverTaskHandle); //suspend ourselves
    }else{
        vTaskSuspend(driverTaskHandle); //suspend driver task
        //then give mutex
        if(xSemaphoreGetMutexHolder( Driver_Mutex ) == currTask){
            xSemaphoreGive(Driver_Mutex);
        }
    }
}
//************** driver crash handling END **************//

//************** driver start and restart ****************//
// Initialize Lora Driver with selected power 
void LoraDriverInit(const RadioConfig config){
    if(driverStarted){
        ESP_LOGE(TAG, "Driver already started. Will attempt to restart");  
        LoraDriverRestart(config);
        return;
    }  
    ESP_LOGI(TAG, "Initializing LoRa Module...");
    //Create driver mutex
    if(Driver_Mutex == NULL){
        Driver_Mutex = xSemaphoreCreateMutexStatic(&Driver_Mutex_Buffer);
    }
    //Create DIO1 interrupt binary semaphore
    if(DIO1_Binary == NULL){
        DIO1_Binary = xSemaphoreCreateBinaryStatic(&DIO1_Binary_Buffer);
    }

    //Check this out for example config:
    // https://github.com/jgromes/RadioLib/blob/master/examples/SX126x/SX126x_Settings/SX126x_Settings.ino

    DRIVER_CHECK(radio.begin(config.Freq_MHz, config.BW_KHz, config.SpreadingFactor, config.codingRate,config.syncWord,
                 config.regulator_target_power, config.preambleLength, config.tcxo_voltage, false, //dont use LDO
                  config.pa_duty, config.hp_max), "radio_begin");

    DRIVER_CHECK(radio.setCurrentLimit(140.0f), "set Current Limit");

    // Both modules utilize DIO2 for RF switching; Ebyte for external T/R CTRL, Wio for internal TX path enable
    DRIVER_CHECK(radio.setDio2AsRfSwitch(true), "set DIO2");

    // Configure RadioLib to call our ISR on DIO1 trigger to recv
    radio.setDio1Action(DIO1_ISR);

    // Create interrupt driven RX/TX task
    driverTaskHandle =xTaskCreateStatic( 
        loraInterruptTask,       //Task function
        "lora_rx_task",          //Task name /* Text name for the task. */
        LORA_STACK_SIZE,  
        (void*) 1,              //No parameter
        LORA_RX_PRIORITY,       //High Priority?
        LORA_Stack,             //Task Stack
        &LORA_Buffer);          //Task struct

    ESP_LOGI(TAG, "LoRa Initialized");   
    driverStarted = true; 
}

void LoraDriverRestart(const RadioConfig config){
    if(!driverStarted){
        ESP_LOGE(TAG, "cant re-start un-started driver. Will assume you meant to start driver");
        LoraDriverInit(config);
        return;
    }
    //grab driver mutex, dont want driver doing anything else rn
    while(xSemaphoreTake(Driver_Mutex, portMAX_DELAY) != pdPASS);

    radio.setDio1Action(NULL);

    //Destroy stuff statically allocated by driver
    vTaskDelete(driverTaskHandle);
    vSemaphoreDelete(DIO1_Binary);
    DIO1_Binary = NULL;

    driverStarted = false;  //make driver want to re-initialize
    LoraDriverInit(config);
    //give mutex back
    xSemaphoreGive(Driver_Mutex);
}
//************** driver start and restart END ****************//

//************** driver interrupt handling ***************//
static void printIrqFlags(uint32_t irq);
static void handleRXInterrupt(uint32_t irq);
static bool validRXIRQ(uint32_t irq);

//interupt pin DIO1 triggers ISR. Sends binary to loraInterruptTask
static void IRAM_ATTR DIO1_ISR() {
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    
    // We notify the "interrupt Task" that something happened
    // This task will be the one to actually read the SPI and dispatch
    if (DIO1_Binary != NULL) {
        xSemaphoreGiveFromISR(DIO1_Binary, &xHigherPriorityTaskWoken);
    }
    
    if (xHigherPriorityTaskWoken) portYIELD_FROM_ISR();
}

//handles interrupts from DIO1_ISR
static void loraInterruptTask(void *pvParameters) {
    while (true) {

        while(xSemaphoreTake(DIO1_Binary, portMAX_DELAY) != pdPASS); //grab interrupt binary when interrupt triggers
        if(!driverGrab()){
            continue;   //driver not started, skip
        }
        //retrieve then clear irq
        uint32_t irq = 0;
        if(driverCheck([&]{return radio.getIrqFlagsSafe(irq);}, "irqRead")){ 
            driverYield(); continue; 
        }
        if(driverCheck([&]{return radio.clearIrqFlags(irq);}, "irqClear")){ 
            driverYield();  continue; 
        }
        printIrqFlags(irq);

        if((irq & RADIOLIB_SX126X_IRQ_RX_DONE) && (irq & RADIOLIB_SX126X_IRQ_TX_DONE)){
            driverCrash(-999, "Simultaneous RX and TX complete IRQ");
        }

        //handles receiving a msg
        if(irq & RADIOLIB_SX126X_IRQ_RX_DONE){
            handleRXInterrupt(irq);
        } else if (irq & RADIOLIB_SX126X_IRQ_TX_DONE){
            driverYield();
            protocolTXComplete();   //tell protocol their TX finished
        }else{
            driverYield();
        }
    }
}

// Print irq flags for debugging.
static void printIrqFlags(uint32_t irq) {
    printf("IRQ Flags (0x%04" PRIX32 "): [", irq);
    for (int i = 31; i >= 0; i--) {
        printf("%u", (unsigned int) ((irq >> i) & 0x01));
        if (i % 4 == 0 && i != 0) printf(" ");
    }
    printf("]\n");
}

// assumes driverMutex is already grabbed. calls protocol recv if able to get successfull packet.
static void handleRXInterrupt(uint32_t irq){
    int16_t state; 
    // 1. Capture Length
    size_t length = radio.getPacketLength();    //stored internally, I dont think error is possible
    
    //log the strength of incomming packet
    ESP_LOGI(TAG, "Packet Recv - RSSI: %.2f dBm, SNR: %.2f dB", 
                radio.getRSSI(), radio.getSNR());
    //choosing not error check these^^. not worth crashing on them, if other things magically work.

    // 2. Range Validation (Sanity Check)
    if (length <= 0 || length > 256) {
        ESP_LOGE(TAG, "Invalid packet length detected: %u", length);
        driverYield();  return;   //ignore packet;
    }

    //read and check data
    if(driverCheck([&]{ state = radio.readData(rxBuffer, length); 
        return (state == RADIOLIB_ERR_CRC_MISMATCH) ? RADIOLIB_ERR_NONE : state; //handle CRC mismatch outside
    }, "readData")){
        ESP_LOGE(TAG, "unexpected readData error!");
        driverYield();  return; 
    }

    if (state == RADIOLIB_ERR_CRC_MISMATCH) {
        ESP_LOGE(TAG, "CRC Mismatch between radioLib computation and chip. SPI cooked?");
        //restart listening and ignore packet
        driverCheck([&]{return radio.startReceive();}, "startRecv");
        driverYield(); return;
    }
    //check if chip computed CRC errors, we dont have a CRC mismatch if we get here:
    if(!validRXIRQ(irq)){
        driverYield(); return;
    }


    //give protocol the received packet
    driverYield();  //yield driver before calling protocol!
    protocolReceive(rxBuffer, length);
}
static bool validRXIRQ(uint32_t irq){
    bool retVal = true;
    if(irq & RADIOLIB_SX126X_IRQ_CRC_ERR){
        ESP_LOGW(TAG, "CRC error");
        retVal=false;
    }
    if(irq & RADIOLIB_SX126X_IRQ_HEADER_ERR){
        ESP_LOGW(TAG, "error with packet header");
        retVal = false;
    }
    if ((~irq) & RADIOLIB_SX126X_IRQ_HEADER_VALID){
        ESP_LOGW(TAG, "recv something that isnt a valid Lora header");
        retVal = false;
    }

    return retVal;
}


//************** driver interrupt handling END ***************//

//************** driver public commands ***************//
static int16_t waitIfReceiving(uint64_t timerExpireTime_us);   //LoraTransmit helper

void LoraStartRecv(){
    if(!driverGrab()){
        return; //driver not started
    }
    DRIVER_CHECK(radio.startReceive(), "loraStartRecv");
    driverYield();
}

uint32_t LoraGetTimeOnAir(){
    return radio.getTimeOnAir(maxLoraPacketSize);
}

//returns either RADIOLIB_ERR_NONE on success, or RADIOLIB_LORA_DETECTED on timeout
//may also raise error to driver
//This will keep trying to start a transmission until timeout, or an error
//If we reach the process of transmiting, this will not terminate due to timeout.
int16_t LoraTransmit(const uint8_t* data, const uint16_t dataLen, const uint64_t timerExpireTime_us) {
    if(!driverGrab()){
        return RADIOLIB_ERR_INVALID_MODE; //indicate driver not started
    }
    int16_t state = RADIOLIB_LORA_DETECTED; //indicate timeout by default

    while(esp_timer_get_time() < timerExpireTime_us){ 
        if( waitIfReceiving(timerExpireTime_us) == RADIOLIB_ERR_NONE){
            //Thorough scan of all activity, interrupts TX/RX
            if(driverCheck([&]{ state = radio.scanChannel(); 
                return (state == RADIOLIB_LORA_DETECTED || state == RADIOLIB_CHANNEL_FREE) ? RADIOLIB_ERR_NONE : state;
            }, "scanChannelTX")){
                driverYield();  return state;
            }

            if(state == RADIOLIB_CHANNEL_FREE) {  //good to go
                ESP_LOGI(TAG, "driver start transmit");
                if(driverCheck([&]{return state = radio.startTransmit(data, dataLen);}, "LoraTransmitStart")){
                    driverYield();  return state;
                }
            }
            if(state != RADIOLIB_LORA_DETECTED){    
                driverYield();
                return state; //we had an actual, unexpected error
            }
        }else{
            //return error if we have something we dont expect
            if(state != RADIOLIB_ERR_NONE && state != RADIOLIB_LORA_DETECTED){
                driverYield();  return state;
            }
        }
        vTaskDelay(pdMS_TO_TICKS(20));
    }
    driverYield();
    if(state == RADIOLIB_ERR_NONE){
        return RADIOLIB_LORA_DETECTED;  //indicate the timeout
    }
    return state;   //indicate the actual error over timeout, if one was risen
}

static int16_t waitIfReceiving(uint64_t timerExpireTime_us) {
    uint32_t irq;
    while(esp_timer_get_time() < timerExpireTime_us){
        //get irq flags to check if we know we are already mid-reception
        int16_t state = RADIOLIB_ERR_NONE;
        if(driverCheck([&]{return state = radio.getIrqFlagsSafe(irq);}, "get_irq_in_wait")){
            return state;
        }

        //check if irq indicates we are recv
        if ( !(irq & RADIOLIB_SX126X_IRQ_PREAMBLE_DETECTED) ||  //we havent started hearing a new message
             (irq & (RADIOLIB_SX126X_IRQ_RX_DONE | RADIOLIB_SX126X_IRQ_CRC_ERR))  //we are done hearing our last message
        ) {
            return RADIOLIB_ERR_NONE;   //we arent currently receiving!
        }
        vTaskDelay(pdMS_TO_TICKS(20)); //wait a bit before polling again
    }
    return RADIOLIB_LORA_DETECTED;    //couldnt get clear
}

