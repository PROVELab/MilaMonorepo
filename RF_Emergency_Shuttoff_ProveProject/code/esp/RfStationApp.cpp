#include "RfStationApp.h"
#include "RfRadio.h"
#include "../rf/rf_station.h"
#include "telemRXMain/UART_COM.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>

static RfStation station;
static portMUX_TYPE lock = portMUX_INITIALIZER_UNLOCKED;

bool rfStationUsbCommand(const uint8_t *p, size_t n) {
    /* The existing UART checksum is checked before this function is called.
       A separate prefix keeps RF controls out of the generated command parser. */
    if (n != 5 || memcmp(p,"RFC1",4) || p[4] > 2) return false;
    portENTER_CRITICAL(&lock);
    rf_station_command(&station,p[4],(uint64_t)esp_timer_get_time());
    portEXIT_CRITICAL(&lock);
    return true;
}

static void stationTask(void *) {
    if (!rfRadioInit()) { vTaskDelete(nullptr); return; }
    for (;;) {
        RfFrame request, reply;
        uint64_t now = (uint64_t)esp_timer_get_time();
        if (!rfRadioRead(request,now+10000)) continue;
        portENTER_CRITICAL(&lock);
        bool valid = rf_station_reply(&station,&request,(uint64_t)esp_timer_get_time(),&reply);
        portEXIT_CRITICAL(&lock);
        if (!valid) continue;
        /* Respond directly, before USB output. No burst ACK and no reply queue. */
        bool sent = rfRadioSend(reply,(uint64_t)esp_timer_get_time()+22000);
        uint8_t status[30] = {'R','F','S','1'};
        rf_encode(&request,status+4);
        status[28] = reply.permit; status[29] = sent ? 1 : 0;
        /* The laptop learns what the CAR reported, plus our local reply result.
           Sending a reply does not prove the car received it. */
        LORA_TO_UART(status,sizeof(status));
    }
}

void rfStationStart() {
    rf_station_init(&station); /* STOP until an explicit allow command. */
    configASSERT(xTaskCreate(stationTask,"rf_station",6144,nullptr,8,nullptr)==pdPASS);
}
