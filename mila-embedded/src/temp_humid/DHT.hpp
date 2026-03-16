/* 
	DHT22 temperature sensor driver
*/

//Code adapted from: https://github.com/Andrey-m/DHT22-lib-for-esp-idf.git

#ifndef DHT_HPP
#define DHT_HPP

#include "driver/gpio.h"

#define DHT_OK 0
#define DHT_CHECKSUM_ERROR -1
#define DHT_TIMEOUT_ERROR -2

class DHT
{
  public:
    DHT();

    void setDHTgpio(gpio_num_t gpio);
    void errorHandler(int response);
    int readDHT();
    float getRH();
    float getTemperature();
    static float getAbsoluteHumidity(float temp, float rh);

  private:
    gpio_num_t DHTgpio;
    float rh = 0.;
    float temperature = 0.;

    int getSignalLevel(int usTimeOut, bool state);
};

#endif