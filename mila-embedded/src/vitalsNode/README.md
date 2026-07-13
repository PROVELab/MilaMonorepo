Vitals' key tasks include:


1. monitoring datam and disabling contactors in emergencies (see vitalsData folder)
2. sendingHB messages over CANBus, and updating telemetry with HB and Vitals' status info (see vitalsHB folder)
3. Communicate with telemetry (using the telemtryTX code). Use vitalsSendData.c to send formatted messages, and the auto-generated callbacks directory to receive telemetry messages. The structure of these messages it declared in packetFormat.py.
4. Sequencing the precharge and powerDistribution contactors, disabling them if in critical state. See contactorControl.c
5. parsing CAN can data frames, which is necessary for vitalsData and telemetry. This is done using the auto-generated vitalsGen folder. (see scripts for how its created)

Main declares all tasks, and then adds all can listen params in receiveMsg, which is responsible for recieving all can messages