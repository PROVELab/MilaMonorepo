Contains logic common to all sensors. 

Place any future logic that is sensor universal in sensorCommon.cpp. Place logic that is specific to microcontroller in sensorSpecifc.cpp. 

Currently, sensorSpecifc.cpp contains logic for esp-idf and Arduino frameworks. The stm embassy rust version is generated into each node crate as `rust/<node>_rust/src/sensor_specific.rs`.
