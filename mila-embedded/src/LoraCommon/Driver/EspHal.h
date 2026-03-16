#ifndef ESP_HAL_H
#define ESP_HAL_H

#include <RadioLib.h>
#include <driver/spi_master.h>
#include <driver/gpio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h"
#include <string.h>

#define LOW                         (0x0)
#define HIGH                        (0x1)
#define INPUT                       (0x01)
#define OUTPUT                      (0x03)
#define RISING                      (0x01)
#define FALLING                     (0x02)
#define NOP()                       asm volatile ("nop")

class EspHal : public RadioLibHal {
  public:
    EspHal(int8_t sck, int8_t miso, int8_t mosi)
      : RadioLibHal(INPUT, OUTPUT, LOW, HIGH, RISING, FALLING),
      spiSCK(sck), spiMISO(miso), spiMOSI(mosi) {
    }

    void init() override { spiBegin(); }
    void term() override { spiEnd(); }

    void pinMode(uint32_t pin, uint32_t mode) override {
      if(pin == RADIOLIB_NC) return;
      gpio_config_t conf = {};
      conf.pin_bit_mask = (1ULL << pin);
      conf.mode = (gpio_mode_t)mode;
      conf.pull_up_en = GPIO_PULLUP_DISABLE;
      conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
      conf.intr_type = GPIO_INTR_DISABLE;
      gpio_config(&conf);
    }

    void digitalWrite(uint32_t pin, uint32_t value) override {
      if(pin == RADIOLIB_NC) return;
      gpio_set_level((gpio_num_t)pin, value);
    }

    uint32_t digitalRead(uint32_t pin) override {
      if(pin == RADIOLIB_NC) return 0;
      return gpio_get_level((gpio_num_t)pin);
    }

    void attachInterrupt(uint32_t interruptNum, void (*interruptCb)(void), uint32_t mode) override {
      if(interruptNum == RADIOLIB_NC) return;
      gpio_install_isr_service(0);
      gpio_set_intr_type((gpio_num_t)interruptNum, (gpio_int_type_t)(mode & 0x7));
      gpio_isr_handler_add((gpio_num_t)interruptNum, (void (*)(void*))interruptCb, NULL);
    }

    void detachInterrupt(uint32_t interruptNum) override {
      if(interruptNum == RADIOLIB_NC) return;
      gpio_isr_handler_remove((gpio_num_t)interruptNum);
    }

    void delay(unsigned long ms) override { vTaskDelay(pdMS_TO_TICKS(ms)); }
    
    void delayMicroseconds(unsigned long us) override {
      uint64_t start = esp_timer_get_time();
      while ((esp_timer_get_time() - start) < us) { NOP(); }
    }

    unsigned long millis() override { return (unsigned long)(esp_timer_get_time() / 1000ULL); }
    unsigned long micros() override { return (unsigned long)(esp_timer_get_time()); }

    long pulseIn(uint32_t pin, uint32_t state, unsigned long timeout) override {
      if(pin == RADIOLIB_NC) return 0;
      uint32_t start = micros();
      while(digitalRead(pin) != state) {
        if(micros() - start > timeout) return 0;
      }
      uint32_t pulseStart = micros();
      while(digitalRead(pin) == state) {
        if(micros() - pulseStart > timeout) return 0;
      }
      return micros() - pulseStart;
    }

    void spiBegin() {
      // Initialize with zeros to satisfy -Wmissing-field-initializers
      spi_bus_config_t buscfg = {};
      buscfg.mosi_io_num = spiMOSI;
      buscfg.miso_io_num = spiMISO;
      buscfg.sclk_io_num = spiSCK;
      buscfg.quadwp_io_num = -1;
      buscfg.quadhd_io_num = -1;
      buscfg.max_transfer_sz = 4096;

      spi_device_interface_config_t devcfg = {};
      devcfg.mode = 0;
      devcfg.clock_speed_hz = 8 * 1000 * 1000; // 8 MHz
      devcfg.spics_io_num = -1;                // Managed manually by Module/Radio
      devcfg.queue_size = 7;

      spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO);
      spi_bus_add_device(SPI2_HOST, &devcfg, &spiHandle);
    }

    void spiTransfer(uint8_t* out, size_t len, uint8_t* in) override {
      if (len == 0) return;
      spi_transaction_t t = {};
      t.length = len * 8;
      t.tx_buffer = out;
      t.rx_buffer = in;
      spi_device_polling_transmit(spiHandle, &t);
    }

    void spiEnd() {
      spi_bus_remove_device(spiHandle);
      spi_bus_free(SPI2_HOST);
    }

    void spiBeginTransaction() override {}
    void spiEndTransaction() override {}

  private:
    int8_t spiSCK;
    int8_t spiMISO;
    int8_t spiMOSI;
    spi_device_handle_t spiHandle;
};

#endif