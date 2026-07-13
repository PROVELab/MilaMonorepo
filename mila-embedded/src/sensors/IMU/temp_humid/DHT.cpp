#define LOG_LOCAL_LEVEL ESP_LOG_DEBUG

#include <esp_log.h>
#include <cmath>
#include "driver/gpio.h"
#include "DHT.hpp"

static char TAG[] = "DHT";

DHT::DHT()
{
    DHTgpio = GPIO_NUM_33;
    rh = 0.;
    temperature = 0.;
}

void DHT::setDHTgpio(gpio_num_t gpio)
{
    DHTgpio = gpio;
}

float DHT::getRH() { return rh; }
float DHT::getTemperature() { return temperature; }

float DHT::getAbsoluteHumidity(float temp, float rh)
{
    return (6.112 * std::exp((17.67 * temp) / (temp + 243.5)) * rh * 2.1674) / (273.15 + temp);
}

void DHT::errorHandler(int response)
{
    switch (response)
    {
    case DHT_TIMEOUT_ERROR:
        ESP_LOGE(TAG, "Sensor Timeout\n");
        break;
    case DHT_CHECKSUM_ERROR:
        ESP_LOGE(TAG, "CheckSum error\n");
        break;
    case DHT_OK:
        break;
    default:
        ESP_LOGE(TAG, "Unknown error\n");
    }
}

int DHT::getSignalLevel(int usTimeOut, bool state)
{
    int uSec = 0;
    while (gpio_get_level(DHTgpio) == state)
    {
        if (uSec > usTimeOut)
            return -1;
        ++uSec;
        esp_rom_delay_us(1);
    }
    return uSec;
}

#define MAXdhtData 5

int DHT::readDHT()
{
    int uSec = 0;
    uint8_t dhtData[MAXdhtData] = {0};
    uint8_t byteInx = 0;
    uint8_t bitInx = 7;

    gpio_set_direction(DHTgpio, GPIO_MODE_OUTPUT);
    gpio_set_level(DHTgpio, 0);
    esp_rom_delay_us(3000);
    gpio_set_level(DHTgpio, 1);
    esp_rom_delay_us(25);
    gpio_set_direction(DHTgpio, GPIO_MODE_INPUT);

    if (getSignalLevel(85, 0) < 0) return DHT_TIMEOUT_ERROR;
    if (getSignalLevel(85, 1) < 0) return DHT_TIMEOUT_ERROR;

    for (int k = 0; k < 40; k++)
    {
        if (getSignalLevel(56, 0) < 0) return DHT_TIMEOUT_ERROR;
        uSec = getSignalLevel(75, 1);
        if (uSec < 0) return DHT_TIMEOUT_ERROR;

        if (uSec > 40)
        {
            dhtData[byteInx] |= (1 << bitInx);
        }

        if (bitInx == 0)
        {
            bitInx = 7;
            ++byteInx;
        }
        else
            bitInx--;
    }

    rh = (dhtData[0] << 8 | dhtData[1]) / 10.0;

    temperature = (dhtData[2] & 0x7F) << 8 | dhtData[3];
    temperature /= 10.0;
    if (dhtData[2] & 0x80)
        temperature *= -1;

    if (dhtData[4] == ((dhtData[0] + dhtData[1] + dhtData[2] + dhtData[3]) & 0xFF))
        return DHT_OK;
    else
        return DHT_CHECKSUM_ERROR;
}