#ifndef SX1262_EXT_H
#define SX1262_EXT_H

#include <RadioLib.h>

// Class name updated to SX1262_Ext
class SX1262_Ext : public SX1262 {
  public:
    // Constructor
    SX1262_Ext(Module* mod);
    using SX1262::begin;  //make OG begin still be accessible

    int16_t begin(float freq, float bw, uint8_t sf, uint8_t cr, uint8_t syncWord, int8_t power,
      uint16_t preambleLength, float tcxoVoltage, bool useRegulatorLDO, uint8_t paDutyCycle, uint8_t hpMax
    );

    // Optimized power function with datasheet-specific PA controls
    int16_t setOutputPowerOptimized(int8_t power, uint8_t paDutyCycle, uint8_t hpMax);
    
    //getIrqFlags with error checking
    int16_t getIrqFlagsSafe(uint32_t &irq);

};

#endif