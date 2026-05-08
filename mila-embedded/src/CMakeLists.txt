
# list(APPEND EXTRA_COMPONENT_DIRS icm20948)
list(APPEND EXTRA_COMPONENT_DIRS icm20948)

if(CMAKE_VITALS_ESP_BUILD)
    file(GLOB VITALS_HELPER_FILES "vitalsNode/vitalsHelper/*")
    file(GLOB VITALS_ROOT_FILES   "vitalsNode/*")
    file(GLOB LORA_COMMON_FILES   "LoraCommon/*")
    file(GLOB LORA_DRIVER_FILES   "LoraCommon/Driver/*")
    file(GLOB LORA_TX_FILES       "telemetryTX/*")

    # 2. Define SRC_FILES with explicit paths and append the globbed lists
    set(SRC_FILES 
        "pecan/common.cpp"
        "pecan/espSpecific.c"
        "espBase/debug_esp.c"
        ${VITALS_HELPER_FILES}
        ${VITALS_ROOT_FILES}
        ${LORA_COMMON_FILES}
        ${LORA_DRIVER_FILES}
        ${LORA_TX_FILES}
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
    # message(SEND_ERROR "Path is set to: ${DECPath}")

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
    file(GLOB_RECURSE MCU_SRC
        CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/espBase/*.c"
        "${CMAKE_CURRENT_SOURCE_DIR}/mcu/*.c"
        "${CMAKE_CURRENT_SOURCE_DIR}/mcu/*.cc"
        "${CMAKE_CURRENT_SOURCE_DIR}/mcu/*.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/mcu/*.cxx"

        "${CMAKE_CURRENT_SOURCE_DIR}/sensors/common/sensorHelper.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/sensors/powerSensor/espPower.c"
        "${CMAKE_CURRENT_SOURCE_DIR}/sensors/pedalSensor/*.c"
    )

    # If you also want to include other existing files outside mcu/, add them here:
    set(SRC_FILES
        ${MCU_SRC} ${SRC_FILES}
    )
endif()

if(CMAKE_TELEM_TX)

    file(GLOB TX_LORA_SRC
        #Need pecan
        "sensors/${SENS_DIR}/*.c"
        "sensors/${SENS_DIR}/*.cpp"
        #
        "telemetryTX/*.c"
        "telemetryTX/*.cpp"
        "LoraCommon/*.c"
        "LoraCommon/*.cpp"
        "LoraCommon/Driver/*.c"
        "LoraCommon/Driver/*.cpp"
    )
    set(SRC_FILES
        ${TX_LORA_SRC}
    )
endif()

if(CMAKE_TELEM_RX)
    file(GLOB TX_LORA_SRC
        #Need pecan
        "sensors/${SENS_DIR}/*.c"
        "sensors/${SENS_DIR}/*.cpp"
        #
        "telemetryRX/**.c"
        "telemetryRX/**.cpp"
        "LoraCommon/*.c"
        "LoraCommon/*.cpp"
        "LoraCommon/Driver/*.c"
        "LoraCommon/Driver/*.cpp"
    )
    set(SRC_FILES
        ${TX_LORA_SRC}
    )
endif()

if(CMAKE_TELEM_Listen)
    file(GLOB RX_LORA_SRC
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
    file(GLOB SIMPLE_TX_LORA_SRC
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
