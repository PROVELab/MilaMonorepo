pecan is PROVE's abstraction layer for sending Can messages. common pecan functionality is in pecan.h. Functoins that interact directly with hardware are custom based on the microcontroller. Thus, espSpecific.c, arduinoSpecific.cpp, and pecan_rust.rs implement this behavior for esps, arduino's, and stm's using emabassy respectively. The pecan.rs one is in mila-embedded/rust/sensor_common/pecan_rust.rs.

Note: Prove has moved away from using Arduinos. Use the arduinoSpecific.cpp at your own risk!

Key features of pecan include: 
1. create a pcan listen params collections (plpc), where you specify which ids map onto certain callback functions. Prove splits the 11 bit packet ids into two parts: a 4 bit function id, and a 7-bit nodeId. the nodeId corresponds to which node is sending the msg. the function id indicates the function of the message. You can choose to have a listen param check for match on the function id, or the entire id (both function and nodeID) using the .match_type parameter
2. Call waitPackets on this plpc to receive packets and map them onto their callback functions.
3. You must set a default_packet_recv callbacks for what to call for packets that do not match onto a plpc callback. pecan provides a defaultPacketRecv that just prints the information, which is good for most cases, where you do not need to do anything for packets you are not expecting.
4. sendPacket provides a universal abstraction for sending CAN packets with the same code, regardless of microcontroller.
5. pecan_pack and unpack are helper functions for how pecan format's its data when sending over CAN. vitals uses a variation of these pack functions that identical behavior wise, but designed for streaming larger packets to telemetry.
6. Helper functions like sendStatusUpdate
7. flexiblePrint: so that each microcontroller can print messages, the specific implementation for that micronctroller defines how to print. For esps this is ESP_LOGI, for rust: rprint!, for arduinos: Serial.println()