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

#include "Driver.hpp"
#include "Config.hpp"

static const char *TAG = "LoraDrive";

driverInfo driver_info = {};
SemaphoreHandle_t driver_info_binary = NULL;
static StaticSemaphore_t driver_info_binary_buffer;


RadioPinout pins = getRadioPins();
//RadioLib Dec
EspHal RadioLibHal = EspHal(pins.sclk, pins.miso, pins.mosi); 
Module mod = Module(&RadioLibHal, pins.nss, pins.dio1, pins.nrst, pins.busy); 
SX1262_Ext radio = SX1262_Ext(&mod);

#define LORA_STACK_SIZE 8192
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

inline bool driverGrab(){
    while(xSemaphoreTake(Driver_Mutex, portMAX_DELAY) != pdTRUE);
    if(driver_info.state == off){   
        ESP_LOGW(TAG, "failed to grab driver, currently crashed!");
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
//indicate if we failed to perform action with false:
#define DRIVER_CHECK_BOOL(expression, msg) if (driverCheck([&]{return expression;}, msg)){ return false;}


//driver Mutex should be grabbed if this is called
static bool resetState(){
    ESP_LOGI(TAG, "resetting driver state per user request");
    driver_info.state = standby;
    DRIVER_CHECK_BOOL(radio.standby(), "enter standby");
    
    //reset irq:
    if(xSemaphoreTake(DIO1_Binary, 0) == pdPASS){   //clear any pending interrupts
        //just throw away this data, unlucky timing.
        uint32_t irq = 0;
        DRIVER_CHECK_BOOL(radio.getIrqFlagsSafe(irq), "irqRead");
        DRIVER_CHECK_BOOL(radio.clearIrqFlags(irq), "irqClear");
    }
    return true;    //successfully reset state
}

//Can be called by user to stop whatever the driver was doing.
//Driver cant do anything until LoraStartRecv or LoraTransmit are called again
void enterStandBy(){
    if(!driverGrab()){
        return;
    }
    //on error, this will release the mutex
    if(resetState()){
        driverYield();
    }
    // driverYield();
}

//used internally when LoraStartRecv or LoraTransmit are called
static void exitStandBy(){
    if(driver_info.state == standby){
        driver_info.state = running;
    }
}

//the driver mutex should be held any time this is called
static void driverCrash(int16_t error, const char* msg){
    if(xSemaphoreGetMutexHolder(Driver_Mutex) != xTaskGetCurrentTaskHandle()){
        driverGrab();
    }
    ESP_LOGE(TAG, "raising driver crash from %s with error %d", msg, error);
    //try to put chip in standby mode. ok if fails, reboot will restart it
    radio.standby();
    uint32_t irq = 0;
    radio.getIrqFlagsSafe(irq);
    radio.clearIrqFlags(irq);
    mod.term();  //terminate radio hardware
    //
    driver_info.state = off;
    driver_info.crashError = error;
    strncpy(driver_info.crashMsg, msg, crashMsgSize);
    driver_info.crashMsg[crashMsgSize - 1] = 0; //force null termination
    xSemaphoreGive(driver_info_binary); //notify protocol we crashed
    //drop the driver mutex if we hold it.
    xSemaphoreGive(Driver_Mutex);
}

//allow the protocol to raise a crash
void raiseDriverCrash(int16_t error, const char* msg){
    driverGrab();
    driverCrash(error, msg);
    driverYield();
}
//************** driver crash handling END **************//

//************** driver start and restart ****************//

// Initialize Lora Driver with selected config. May be called again at any time to re-start the driver
void LoraDriverInit(const RadioConfig* config){
    if(driver_info.state != off){
        ESP_LOGE(TAG, "Error, attempt to start driver when driver is not in off state");
        return;
    }
    ESP_LOGI(TAG, "Initializing LoRa Driver...");
    //Create driver mutex
    if(Driver_Mutex == NULL){
        Driver_Mutex = xSemaphoreCreateMutexStatic(&Driver_Mutex_Buffer);
    }
    //Create DIO1 interrupt binary semaphore
    if(DIO1_Binary == NULL){
        DIO1_Binary = xSemaphoreCreateBinaryStatic(&DIO1_Binary_Buffer);
    }

    //give to user whenever state changes
    if(driver_info_binary == NULL){
        driver_info_binary = xSemaphoreCreateBinaryStatic(&driver_info_binary_buffer);
    }
    //ensure nothing else on driver is running during init
    xSemaphoreTake(Driver_Mutex, portMAX_DELAY); 

    //Check this out for example config:
    // https://github.com/jgromes/RadioLib/blob/master/examples/SX126x/SX126x_Settings/SX126x_Settings.ino

    //DIO1 int pin should go low, radio.begin includes a hard reset on the SX chip
    DRIVER_CHECK(radio.begin(config->Freq_MHz, config->BW_KHz, config->SpreadingFactor, config->codingRate,
                             config->syncWord, config->regulator_target_power, config->preambleLength, 
                             config->tcxo_voltage, false, config->pa_duty, config->hp_max), "radio_begin");
                                                   //^dont use LDO

    DRIVER_CHECK(radio.setCurrentLimit(140.0f), "set Current Limit");

    // Both modules utilize DIO2 for RF switching; Ebyte for external T/R CTRL, Wio for internal TX path enable
    DRIVER_CHECK(radio.setDio2AsRfSwitch(true), "set DIO2");

    // Configure RadioLib to call our ISR on DIO1 trigger to recv
    radio.setDio1Action(DIO1_ISR);

    // Create interrupt driven RX/TX task
    if(driverTaskHandle == NULL){
        driverTaskHandle = xTaskCreateStatic( 
            loraInterruptTask,       //Task function
            "lora_rx_task",          //Task name /* Text name for the task. */
            LORA_STACK_SIZE,  
            (void*) 1,              //No parameter
            LORA_RX_PRIORITY,       //High Priority?
            LORA_Stack,             //Task Stack
            &LORA_Buffer);          //Task struct
    }
    memset(&driver_info, 0, sizeof(driverInfo));   //clear state on init 
    driver_info.state = running;

    xSemaphoreGive(driver_info_binary);    //notify protocol we started
    xSemaphoreGive(Driver_Mutex); //give mutex back
}

//************** driver interrupt handling ***************//
static bool handleRXInterrupt(uint32_t irq);
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
        if(driver_info.state == standby){
            driverYield();  continue;   //if we intend to be in standby, skip interrupts
        }
        //retrieve then clear irq
        uint32_t irq = 0;
        if(driverCheck([&]{return radio.getIrqFlagsSafe(irq);}, "irqRead")){ 
            continue; 
        }
        if(driverCheck([&]{return radio.clearIrqFlags(irq);}, "irqClear")){ 
            continue; 
        }
        // printIrqFlags(irq);

        //handles receiving a msg
        if(irq & RADIOLIB_SX126X_IRQ_RX_DONE){
            driver_info.recvPacket.RSSI = radio.getRSSI();
            driver_info.recvPacket.SNR = radio.getSNR();
            //choosing not to error check these^^. not worth crashing on them, if other things magically work.

            if (!handleRXInterrupt(irq)) {
                //there was an issue with the packet
                if(driver_info.state == off){
                    continue;   //if we crashed, stop running
                }
                if(!driverCheck([&]{return radio.startReceive();}, "restart recv after bad packet")){
                    driverYield();  // yield if driverCheck didn't do it already
                }
                continue;  //restart loop
            }
            driver_info.recvPacketReady = true;
        } 

        driver_info.recvPacket.irqFlags = irq;   //store irq flags for protocol to read when it gets the driver mutex
        if(irq != 0){
            xSemaphoreGive(driver_info_binary); //notify of state change
        }
        driverYield();
    }
}

// non-reentrant cuz static packet
//returns true if packet looks good. false otherwise
static bool handleRXInterrupt(uint32_t irq){
    // First, check the IRQ flags for errors. If the header or CRC is bad, there's no point proceeding.
    if(!validRXIRQ(irq)){
        return false;
    }

    int16_t state; 
    // 1. Capture Length
    driver_info.recvPacket.dataSize = radio.getPacketLength();    //stored internally, I dont think error is possible

    // 2. Range Validation (Sanity Check)
    if (driver_info.recvPacket.dataSize <= 0 || driver_info.recvPacket.dataSize > 256) {
        ESP_LOGE(TAG, "Invalid packet length detected: %zu", driver_info.recvPacket.dataSize);
        return false;   //ignore packet;
    }

    //read and check data
    if(driverCheck([&]{ state = radio.readData(driver_info.recvPacket.data, driver_info.recvPacket.dataSize); 
        return (state == RADIOLIB_ERR_CRC_MISMATCH) ? RADIOLIB_ERR_NONE : state; //handle CRC mismatch outside
    }, "readData")){
        ESP_LOGE(TAG, "unexpected readData error: %d", state);
        return false;   //ignore packet;
    }

    if (state == RADIOLIB_ERR_CRC_MISMATCH) {
        ESP_LOGE(TAG, "CRC Mismatch between radioLib computation and chip. SPI cooked?");
        return false;
        
    }
    return true;
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
int16_t waitIfReceiving(uint64_t timerExpireTime_us);   //LoraTransmit helper

void LoraStartRecv(){
    if(!driverGrab()){
        return; //driver not started
    }
    exitStandBy();
    DRIVER_CHECK(radio.startReceive(), "loraStartRecv");
    driverYield();
}

uint32_t LoraGetTimeOnAir(){
    return radio.getTimeOnAir(maxLoraPacketSize);
}

#define DRIVER_DELAY(time_ms) do { \
    driverYield();  \
    vTaskDelay(pdMS_TO_TICKS(time_ms)); \
    if(!driverGrab()){ \
        return RADIOLIB_ERR_INVALID_MODE; \
    } \
} while(0)

//returns either RADIOLIB_ERR_NONE on success, or RADIOLIB_LORA_DETECTED on timeout
//may also raise error to driver
//This will keep trying to start a transmission until timeout, or an error
//If we reach the process of transmiting, this will not terminate due to timeout.
int16_t LoraTransmit(const driverSendPacket* packet, const uint64_t timerExpireTime_us) {
    if(!driverGrab()){
        return RADIOLIB_ERR_INVALID_MODE; //indicate driver not started
    }
    exitStandBy();
    int16_t state = RADIOLIB_LORA_DETECTED; //indicate timeout by default
    ESP_LOGI(TAG, "starting transmit");

    while(esp_timer_get_time() < timerExpireTime_us){ 
        //Thorough scan of all activity, interrupts TX/RX
        if(driverCheck([&]{ state = radio.scanChannel(); //only throw if not FREE or LORA_DETECTED
            return (state == RADIOLIB_LORA_DETECTED || state == RADIOLIB_CHANNEL_FREE) ? RADIOLIB_ERR_NONE : state;
        }, "scanChannelTX")){
            return state;
        }

        if(state == RADIOLIB_CHANNEL_FREE) {
            //ok to transmit:
            ESP_LOGI(TAG, "driver start transmit");
            if(driverCheck([&]{return state = radio.startTransmit(packet->data, packet->dataSize);}, "LoraTransmitStart")){
                return state;
            }
            //successfully queued for transmit
            driverYield();
            return state;
        }

        if(state != RADIOLIB_ERR_NONE && state != RADIOLIB_LORA_DETECTED){
            //waitIfReceiving had an issue:
            ESP_LOGW(TAG, "unxepected state");
            return state;
        }
        DRIVER_DELAY(20);
    }
    driverYield();
    if(state == RADIOLIB_ERR_NONE){
        return RADIOLIB_LORA_DETECTED;  //indicate the timeout
    }
    return state;   //indicate the actual error over timeout, if one was risen
}

int16_t waitIfReceiving(uint64_t timerExpireTime_us) {
    if(driver_info.state == off) {
        return RADIOLIB_ERR_INVALID_MODE; //indicate driver not started
    }
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
        DRIVER_DELAY(20); //wait a bit before polling again
    }
    return RADIOLIB_LORA_DETECTED;
}

//return NULL on timeout, or driverInfo if action happened
driverInfo* waitForDriverAction(uint32_t timeout_us){
    if(driver_info.state == off) {
        return NULL;    //driver not started
    }
    if(xSemaphoreTake(driver_info_binary, pdMS_TO_TICKS(timeout_us / 1000)) == pdTRUE){
        return &driver_info;
    }
    return NULL;
}

//only guaranteed to not be modified if the driver crashed
//can be used to pull info from anywhere when the driver crashes
driverInfo* getDriverInfo(){
    return &driver_info;
}

driverInfo* waitForRecv(uint64_t timerExpireTime_us){
    if(driver_info.state == off) {
        return NULL;
    }
    uint64_t currTime = esp_timer_get_time();

    uint32_t timeOutDuration_us = currTime >= timerExpireTime_us ? 0    //avoid overflow
        : ( (timerExpireTime_us - currTime)); //absolute -> relative timeout time
    // LoraStartRecv();

    driverInfo* info = waitForDriverAction(timeOutDuration_us);
    if(info == NULL){
        ESP_LOGW(TAG, "Timeout waiting for driver action on recv");
        //perform one last check on waitIfReceiving
        if(waitIfReceiving(timerExpireTime_us) != RADIOLIB_ERR_NONE){ 
            enterStandBy();
            return NULL;
        }
        //not currently recieving! check one last time:
        info = waitForDriverAction(0);
        if(info == NULL){                     //give up trying to listen for last packet, likely missed it.
            enterStandBy();
            return NULL;
        }
    }
    if(info->state == off){//tell user driver is off
        ESP_LOGW(TAG, "call to waitForRecv with crashed driver. protocol should hanlde crash");
    }
    return info;
}

bool waitForTXDone(uint8_t numPacketTimes) {
    if(driver_info.state == off) {
        return false;
    }
    uint64_t current_time = esp_timer_get_time();
    uint64_t wait_expire_time_us = current_time + (LoraGetTimeOnAir() * numPacketTimes);

    while (current_time < wait_expire_time_us) {
        uint32_t remaining_wait_us = wait_expire_time_us - current_time;
        driverInfo* info = waitForDriverAction(remaining_wait_us);
        if(info == NULL){   //check for timeout
            ESP_LOGE(TAG, "Timed out waiting for TX_DONE.");
            return false; // Timeout
        }
        if(info->state == off){//check for crash
            ESP_LOGW(TAG, "call to waitForTXDone with crashed driver. protocol should hanlde crash");
            return false;
        }
        if (info != NULL && (info->recvPacket.irqFlags & RADIOLIB_SX126X_IRQ_TX_DONE)) {
            ESP_LOGI(TAG, "TX_DONE received.");
            return true; // Success
        }
        if (info != NULL) {
            //this happens like basically every time an interrupt is triggered
            //the IO pin seems to go high before the irq is ready
            ESP_LOGD(TAG, "Woke up for non-TX_DONE IRQ: 0x%04zX while waiting for TX to complete.", info->recvPacket.irqFlags);
        }
        current_time = esp_timer_get_time();
    }
    ESP_LOGE(TAG, "Timed out waiting for TX_DONE.");
    return false; // Timeout
}
