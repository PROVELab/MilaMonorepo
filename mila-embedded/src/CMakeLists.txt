
# list(APPEND EXTRA_COMPONENT_DIRS icm20948)
list(APPEND EXTRA_COMPONENT_DIRS icm20948)

if(CMAKE_VITALS_ESP_BUILD)
    #     file(GLOB TX_LORA_SRC
    #     #Need pecan
    #     "sensors/${SENS_DIR}/*.c"
    #     "sensors/${SENS_DIR}/*.cpp"
    #     #
    #     "telemetryTX/*.c"
    #     "telemetryTX/*.cpp"
    #     "LoraCommon/*.c"
    #     "LoraCommon/*.cpp"
    #     "LoraCommon/Driver/*.c"
    #     "LoraCommon/Driver/*.cpp"
    # )
    # set(SRC_FILES
    #     ${TX_LORA_SRC}
    # )
    file(GLOB VITALS_SRC
        "vitalsNode/*.c"
        "vitalsNode/*.cpp"
        "vitalsNode/vitalsHelper/*.c"
        "vitalsNode/vitalsHelper/*.cpp"
        "vitalsNode/callbacks/*.c"
        "vitalsNode/callbacks/*.cpp"
        "pecan/common.cpp"
        "pecan/espSpecific.c"
        "pecan/vitalsInit.c"
        "espBase/debug_esp.c"
        "telemTX/*.c"
        "telemTX/*.cpp"
        "LoraCommon/*.c"
        "LoraCommon/*.cpp"
        "LoraCommon/Driver/*.c"
        "LoraCommon/Driver/*.cpp"
    )
    set(SRC_FILES
        ${VITALS_SRC}
    )

    # message(SEND_ERROR "SRC_FILES for VITALS_BUILD: ${SRC_FILES}")
endif()

if(CMAKE_HB_ONLY_BUILD)
    set(SRC_FILES 
        "HB_Only_Test/main.c"
        "pecan/common.cpp"
        "pecan/espSpecific.c"
        "espBase/debug_esp.c"
    )
endif()

if(CMAKE_SENSOR_ESP_BUILD)  #CMAKE_SENSOR_ESP_BUILD <- Assumed

    file(GLOB SENSOR_SRC_FILES
        "sensors/${SENS_DIR}/*.c"
        "sensors/${SENS_DIR}/*.cpp"
    )
    set(SRC_FILES
        ${SENSOR_SRC_FILES}
        
        "sensors/common/sensorHelper.cpp"
        "pecan/common.cpp"
        "pecan/espSpecific.c"
        "pecan/vitalsInit.c"
        "espBase/debug_esp.c"
        # "espMutexes/mutex_declarations.c"
    )
    # message(SEND_ERROR "DECPATH: ${DECPATH}")
    # message(SEND_ERROR "Source files: ${SRC_FILES}")
endif()

if(CMAKE_MCU_ESP_BUILD)
    # message(SEND_ERROR "CMAKE_MCU_ESP_BUILD is set, using MCU ESP build configuration")
    # Recursively collect sources under mcu/ (makes stuff easier)
    file(GLOB_RECURSE MCU_SRC_BASE
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
        ${MCU_SRC_BASE} ${MCU_SENSOR_SRC} ${SRC_FILES}
    )
endif()

if(CMAKE_TELEM_TX_BENCHMARK)

    file(GLOB TX_LORA_SRC
        #Need pecan
        "sensors/${SENS_DIR}/*.c"
        "sensors/${SENS_DIR}/*.cpp"
        #
        telemTXBenchmark/*.c
        telemTXBenchmark/*.cpp
        "telemTX/*.c"
        "telemTX/*.cpp"
        "LoraCommon/*.c"
        "LoraCommon/*.cpp"
        "LoraCommon/Driver/*.c"
        "LoraCommon/Driver/*.cpp"
    )
    set(SRC_FILES
        ${TX_LORA_SRC}
    )
endif()

if(CMAKE_TELEM_RX_MAIN)
    file(GLOB_RECURSE RX_LORA_SRC
        #Need pecan
        # "sensors/${SENS_DIR}/*.c"
        # "sensors/${SENS_DIR}/*.cpp"
        #
        "telemRXMain/*.c"
        "telemRXMain/*.cpp"
        "telemRX/*.c"
        "telemRX/*.cpp"
        "LoraCommon/*.c"
        "LoraCommon/*.cpp"
        "LoraCommon/Driver/*.c"
        "LoraCommon/Driver/*.cpp"
    )
    set(SRC_FILES
        ${RX_LORA_SRC}
    )
endif()

if(CMAKE_TELEM_RX_BENCHMARK)
    file(GLOB_RECURSE RX_LORA_SRC
        #Need pecan
        # "sensors/${SENS_DIR}/*.c"
        # "sensors/${SENS_DIR}/*.cpp"
        #
        "telemRXBenchmark/*.c"
        "telemRXBenchmark/*.cpp"
        "telemRX/*.c"
        "telemRX/*.cpp"
        "LoraCommon/*.c"
        "LoraCommon/*.cpp"
        "LoraCommon/Driver/*.c"
        "LoraCommon/Driver/*.cpp"
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
        # "LoraCommon/*.c"
        # "LoraCommon/*.cpp"
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
        "pecan/common.cpp"
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

if(CMAKE_COOLANT_PWM)
    file(GLOB COOLANT_SRC
        "coolantPWM/*.cpp"
        "coolantPWM/*.c"
    )
    set(SRC_FILES
        ${COOLANT_SRC}
    )
endif()

# else()  #assumed "CMAKE_BASIC_NODE" configuration, just using one path
#     set(SRC_FILES
#         "${CMAKE_SRC_FILE}"
#     )   
# endif()
idf_component_register(SRCS ${SRC_FILES} INCLUDE_DIRS ".")