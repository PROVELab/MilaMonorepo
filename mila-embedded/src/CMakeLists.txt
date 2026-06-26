
# Initialize a variable to hold component requirements
set(COMPONENT_REQUIRES "")

if(CMAKE_VITALS_ESP_BUILD)
    file(GLOB_RECURSE VITALS_SRC
        "vitalsNode/*.c"
        "vitalsNode/*.cpp"
        "pecan/common.c"
        "pecan/espSpecific.c"
        "pecan/vitalsInit.c"
        "espBase/debug_esp.c"
        "telemTX/*.c"
        "telemTX/*.cpp"
        "LoraCommon/*.c"
        "LoraCommon/*.cpp"
        # "LoraCommon/Driver/*.c"
        # "LoraCommon/Driver/*.cpp"
    )
    set(SRC_FILES
        ${VITALS_SRC}
    )

    # message(SEND_ERROR "SRC_FILES for VITALS_BUILD: ${SRC_FILES}")
endif()

if(CMAKE_SENSOR_ESP_BUILD) 

    file(GLOB SENSOR_SRC_FILES
        "sensors/${SENS_DIR}/**/*.c"
        "sensors/${SENS_DIR}/**/*.cpp"
        "sensors/${SENS_DIR}/*.c"
        "sensors/${SENS_DIR}/*.cpp"
    )
    set(SRC_FILES
        ${SENSOR_SRC_FILES}
        
        "sensors/common/sensorSpecific.cpp"
        "sensors/common/sensorCommon.cpp"
        "pecan/common.c"
        "pecan/espSpecific.c"
        "pecan/vitalsInit.c"
        "espBase/debug_esp.c"
    )
endif()

if(SENS_DIR STREQUAL "powerDistribution")
    list(APPEND SRC_FILES "sensors/powerSensor/espPower.c")
endif()

if(SENS_DIR STREQUAL "IMU")
    # list(APPEND EXTRA_COMPONENT_DIRS icm20948)
    list(APPEND COMPONENT_REQUIRES "icm20948" "i2c" "log" "esp_timer" "filters")
endif()

if(CMAKE_MCU_ESP_BUILD)
    # message(SEND_ERROR "CMAKE_MCU_ESP_BUILD is set, using MCU ESP build configuration")
    # Recursively collect sources under mcu/ (makes stuff easier)
    file(GLOB_RECURSE MCU_SRC
        FOLLOW_SYMLINKS
        CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/espBase/*.c"
        "${CMAKE_CURRENT_SOURCE_DIR}/mcu/*.c"
        "${CMAKE_CURRENT_SOURCE_DIR}/mcu/*.cc"
        "${CMAKE_CURRENT_SOURCE_DIR}/mcu/*.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/mcu/*.cxx"

        "${CMAKE_CURRENT_SOURCE_DIR}/sensors/common/sensorHelper.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/sensors/powerSensor/espPower.c"
    )
    file(GLOB MCU_SENSOR_SRC
        "${CMAKE_CURRENT_SOURCE_DIR}/sensors/${SENS_DIR}/*.c"
        "${CMAKE_CURRENT_SOURCE_DIR}/sensors/${SENS_DIR}/*.cpp"
    )

    # If you also want to include other existing files outside mcu/, add them here:
    set(SRC_FILES
        ${MCU_SRC} ${MCU_SENSOR_SRC} ${SRC_FILES}
    )
endif()

if(CMAKE_TELEM_TX_BENCHMARK)

    file(GLOB TX_LORA_SRC
        #Need pecan
        "sensors/${SENS_DIR}/*.c"
        "sensors/${SENS_DIR}/*.cpp"
        #
        "telemTXBenchmark/*.c"
        "telemTXBenchmark/*.cpp"
        "telemTX/*.c"
        "telemTX/*.cpp"
        "LoraCommon/*.c"
        "LoraCommon/*.cpp"
        "LoraCommon/Driver/*.c"
        "LoraCommon/Driver/*.cpp"
        "vitalsNode/vitalsGen/vitalsPacketSendLUT.c"
        "vitalsNode/loraStaticHelper/vitalsSendData.c"
        "pecan/common.c"
        "pecan/espSpecific.c"
    )
    set(SRC_FILES
        ${TX_LORA_SRC}
    )
endif()

if(CMAKE_TELEM_RX_MAIN)
    file(GLOB_RECURSE RX_LORA_SRC
        "telemRXMain/*.c"
        "telemRXMain/*.cpp"
        "telemRX/*.c"
        "telemRX/*.cpp"
        "LoraCommon/*.c"
        "LoraCommon/*.cpp"
        "LoraCommon/Driver/*.c"
        "LoraCommon/Driver/*.cpp"
        "vitalsNode/vitalsGen/vitalsPacketSendLUT.c"
        "vitalsNode/loraStaticHelper/vitalsSendData.c"

    )
    set(SRC_FILES
        ${RX_LORA_SRC}
    )
endif()

if(CMAKE_TELEM_RX_BENCHMARK)
    file(GLOB_RECURSE RX_LORA_SRC
        "telemRXBenchmark/*.c"
        "telemRXBenchmark/*.cpp"
        "telemRX/*.c"
        "telemRX/*.cpp"
        "LoraCommon/*.c"
        "LoraCommon/*.cpp"
        "LoraCommon/Driver/*.c"
        "LoraCommon/Driver/*.cpp"
        "vitalsNode/vitalsGen/vitalsPacketSendLUT.c"
        "vitalsNode/loraStaticHelper/vitalsSendData.c"
    )
    set(SRC_FILES
        ${RX_LORA_SRC}
    )
endif()


if(CMAKE_TELEM_Listen)
    file(GLOB_RECURSE RX_LORA_SRC
        "LoraSimpleListen/*.c"
        "LoraSimpleListen/*.cpp"
        "LoraCommon/*.c"
        "LoraCommon/*.cpp"
        "LoraCommon/Driver/*.c"
        "LoraCommon/Driver/*.cpp"
    )
    set(SRC_FILES
        ${RX_LORA_SRC}
    )
endif()

if(CMAKE_TELEM_SIMPLE_TRANSMIT)
    file(GLOB_RECURSE SIMPLE_TX_LORA_SRC
        "LoraTestTransmit/*.c"
        "LoraTestTransmit/*.cpp"
        "LoraCommon/*.c"
        "LoraCommon/*.cpp"
        "LoraCommon/Driver/*.c"
        "LoraCommon/Driver/*.cpp"
    )
    set(SRC_FILES
        ${SIMPLE_TX_LORA_SRC}
    )
endif()

if(CMAKE_ESP_LISTEN_BUILD)
    set(SRC_FILES
        "listenCAN/main.c"
        "pecan/common.c"
        "pecan/espSpecific.c"
        "pecan/vitalsInit.c"
        "espBase/debug_esp.c"
    )
endif()

if(CMAKE_BMS)
    file(GLOB BMS_SRC
        "BMS_Test/*.c"
    )
    set(SRC_FILES
        ${BMS_SRC}
    )
endif()
if(CMAKE_HUMID_SENSE)
    file(GLOB BMS_SRC
        "temp_humid/*.cpp"
    )
    set(SRC_FILES
        ${BMS_SRC}
    )
endif()

idf_component_register(
    SRCS ${SRC_FILES} 
    INCLUDE_DIRS "."
    REQUIRES ${COMPONENT_REQUIRES}
)
