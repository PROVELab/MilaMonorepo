#include "SX1262_Ext.hpp"
#include "esp_log.h" 

//For SX1262, deviceSel is 0x00, and paLut is 0x01 per DS Table 13-21
#define SX1262_deviceSEL 0
#define SX1262_paLut 1

// Pass the module pointer up to the parent SX1262 class
SX1262_Ext::SX1262_Ext(Module* mod) : SX1262(mod) {}


int16_t SX1262_Ext::setOutputPowerOptimized(int8_t power, uint8_t paDutyCycle, uint8_t hpMax) {
  // 1. Check if power is within physical limits
  int16_t state = this->checkOutputPower(power, NULL);
  if(state != RADIOLIB_ERR_NONE) return state;

  // 2. Set the Hardware PA configuration
    state = this->setPaConfig(paDutyCycle, SX1262_deviceSEL, hpMax, SX1262_paLut);
  // state = this->setPaConfig(paDutyCycle, hpMax, 0x00, 0x01); //bruh. im lowkeenuienly aretarted
  if(state != RADIOLIB_ERR_NONE) return state;

  // 3. Set the Software TX parameters (The "Power" and "Ramp Time")
  // We call the low-level setTxParams to avoid the high-level override loop
  return this->setTxParams(power, RADIOLIB_SX126X_PA_RAMP_200U);
}
static const char *TAG = "SX_Ext";

int16_t SX1262_Ext::begin(float freq, float bw, uint8_t sf, uint8_t cr, uint8_t syncWord, int8_t power,
  uint16_t preambleLength, float tcxoVoltage, bool useRegulatorLDO, uint8_t paDutyCycle, uint8_t hpMax
) {
  // execute common part
  ESP_LOGI(TAG, "Starting SX1262_Ext::begin ");
  int16_t state = SX126x::begin(cr, syncWord, preambleLength, tcxoVoltage, useRegulatorLDO);
  ESP_LOGI(TAG, "begin returned with state %d", state);
  RADIOLIB_ASSERT(state);

  // configure publicly accessible settings
  state = setSpreadingFactor(sf);
  RADIOLIB_ASSERT(state);

  state = setBandwidth(bw);
  RADIOLIB_ASSERT(state);

  state = setFrequency(freq);
  RADIOLIB_ASSERT(state);

  //we should add this if we end up getting that PaClamping is triggering,
  // and we dont have a means of getting/finding a better matching antenna: 
    // state = SX126x::fixPaClamping();  //dont clamp if antenna is cooked. SX datasheet suggests this
    // RADIOLIB_ASSERT(state);

  state = setOutputPowerOptimized(power, paDutyCycle, hpMax);
  RADIOLIB_ASSERT(state);

  return(state);
}

int16_t SX1262_Ext::getIrqFlagsSafe(uint32_t &irq) {
  //this is precisely the getIrqFlags functions, except we check the state.
  uint8_t data[] = { 0x00, 0x00 };
  int16_t state = this->getMod()->SPIreadStream(RADIOLIB_SX126X_CMD_GET_IRQ_STATUS, data, 2);
  irq = (((uint32_t)(data[0]) << 8) | data[1]);

  return state;
}