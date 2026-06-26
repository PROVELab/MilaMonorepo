Contains logic common to all sensors. 

Place any future logic that is sensor universal in sensorCommon.cpp. Place logic that is specific to microcontroller in sensorSpecifc.cpp. 

Currently, sensorSpecifc.cpp contains logic for esp-idf and Arduino frameworks. The sensorSpecific version for stm embassy rust is present in rust/sensor_common/src/sensor_specifc.rs