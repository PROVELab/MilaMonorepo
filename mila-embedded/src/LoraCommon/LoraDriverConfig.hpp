#include <cstdint>
#pragma once

#define maxLoraPacketSize 255

#ifdef __cplusplus
    #define FN_CONSTEXPR constexpr
#else
    #define FN_CONSTEXPR
#endif

// Definitions for input choices
enum class BoardType {
    Ebyte_SX1262,
    Wio_SX1262
};

enum class TestMode {
    lowPower,
    highPower 
};

// The Configuration Struct
struct RadioConfig {
    // --- LoRa / Link Defaults ---
    float Freq_MHz;
    float BW_KHz;
    uint8_t SpreadingFactor;
    uint8_t codingRate; //set to (4/codingRate)
    uint8_t syncWord;
    uint16_t preambleLength;

    // --- Hardware / Power Specifics (Calculated) ---
    float tcxo_voltage;
    uint8_t pa_duty;
    uint8_t hp_max;
    uint8_t regulator_target_power;
};

//no reason we should be chaging the fixed default ones.
//can update the power setting later SX1262_Ext::setOutputPowerOptimized
FN_CONSTEXPR RadioConfig getStandardConfig(const BoardType board, const TestMode mode) {
    RadioConfig cfg;

    // 1. Apply Fixed Defaults
    cfg.Freq_MHz = 915.0f;  //Lora Frequency
    cfg.BW_KHz = 250.0f;    //BW and SF for ~270ms to transmit 255 byte packets, with -121dBm sensitivity on SX1262
    cfg.SpreadingFactor = 7;
    cfg.codingRate = 7;     //gives 1 bit of FEC for every 7 bits transmitted
    cfg.syncWord = 18;      //For public use
    cfg.preambleLength = 8; //standard preamble length

    // 2. Calculate TCXO Voltage (Board Dependent)
    if (board == BoardType::Ebyte_SX1262) {
        cfg.tcxo_voltage = 1.8f;
    } else {
        cfg.tcxo_voltage = 2.2f; // Wio safe value
    }

    // 3. Calculate Power Brackets (Mode Dependent)
    if (mode == TestMode::lowPower) {   //for testing on a desk. range will be terrible!
        cfg.pa_duty = 2;
        cfg.hp_max = 2;
        cfg.regulator_target_power = 8;        
    } 
    else {
        // High Power Operation (for use on car)
        if (board == BoardType::Ebyte_SX1262) { //Ebyte need 17 feet space (in final setup)
            //need >17 feet distance
            cfg.pa_duty = 4;
            cfg.hp_max = 7;
            cfg.pa_duty = 2;
            cfg.hp_max = 3;
            cfg.regulator_target_power = 22;
        } else {    
            // Wio need >4 feet distance for 5dbi<->5dbi
            cfg.pa_duty = 4;
            cfg.hp_max = 7;
            cfg.regulator_target_power = 22;
        }
    }
    return cfg;
}

// The container for your pinout
struct RadioPinout {
    int sclk;
    int miso;
    int mosi;
    int nss;
    int nrst;
    int dio1;
    int busy;
};

FN_CONSTEXPR RadioPinout getRadioPins() {
    return RadioPinout {
        // IO pins for SPI bus. Put 100 ohm resistors between SPI pin and chip for EBYTE. (datasheet says 200 ohm, i dont beleive it.)
        .sclk = 25,
        .miso = 26,
        .mosi = 27,
        .nss  = 14, //output SPI chip select

        //output pins for chip state
        .nrst = 13, //hardware reset pin
        .dio1 = 34, //interrupt trigger
        .busy = 35  //indicates when chip is busy processing
    };
}


//schematic summary for EBYTE
//the ones mentioned in getRadioPins, and: 
//T/R CTRL <-> DIO2 (chip-chip connection)
//Enable <-> 3.3V
//Vcc is 5.1V for Ebyte (5.5 absolute max), ideally always just above 5.
//Resistor is 10k, capacitors are 100nF. Neither R nor C specified anywhere. Hopefully these values are ok. SX seems to use similar values in other parts of their chips. (tbh I have no clue)

//Pin uses: //SCLK, Miso, and Mosi. Spi communication with the chip
            //NSS = SPI chip select 
            //DIO1 = used for interrupt.
            //NRST = enables the chip. We could perhaps wire this to vdd at some point
            // Busy = used to determine when chip done processing so can send next SPI command

//If not in Ebyte mode, then need to wire up RX_EN and TX_EN to GPIO pins. Also maybe dont have resistors on SPI lines?
