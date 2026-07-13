This directory contains python scripts to support mila embedded

1. vitals_telem: The main codegen used for the mila-embedded car. The config directory contains information for the codeGen. Currently, enum.def contains global constants and enums for all nodes. The simpleTest.def contains data describing every datapoint and CANFrame collected by every sensors. This code generates LUTs for the sensors and vitals to collect, send, and receive information. Auto-generated code, like that in vitalsHelper (for vitals), and the helper directory of each sensor is not intended for direct modification; the lookup tables should be updated by changing the codegen. 

    All dataFields for both sensors and vitals <-> telem packets must fit in an int32_t. Supporting additional datatypes isnt really worth it. For example: you should convert a float for something like Volts to milliVolts, and then send the milliVolts value as an integer.

    Run the codeGen by running "python3 codeGen_main.py" in the directory. If specific sensors have already had their codeGen completed, you will be prompted to skip, copy, or overwrite for each sensor. Skip if you do not want to do anything for that sensor, copy to create a new version called copy that uses the latest codeGen settings, but leave the current sensor implementation alone. Overwrite to erase what was previously there for the sensor, and replace with the new codeGen skeleton for that sensor. Note: the codeGen for each sensor creates a skeleton for the main of that sensor, so the sensor can compile as is. This means if you select "o" or "oa" (for overwrite all), it will erase the current main of your sensor.

    The sensor's codeGen needs to be in sync with vitals and telemetry. If you change the .def files, you need to ensure all three have their codeGen re-run. That means potentially running the sensor's codegen with "copy," and moving your custom logic for that sensor over to the new copy. Information on parsing the sensor's information is stored in vitalsGen/staticDec for vitals, and inside the telemetry.csv for telemtry (which is in the telem-dashboard directory, not the mila-embedded one)

    You may also configure the custom messages sent between vitals and telemetry by updating the LUT in ./vitals_telem/Lora_Msgs_And_Cms/packetFormat.py
    Parameter description:

        a. mask_bits: how many bits to use for the mask. Use pack_minimum to use the minimum number of bits that do not force your message to use an extra byte. All messages sent between telemetry and Vitals are byte aligned, so if you msg header is 10 bits, the mask will be set to 6 bits automatically using this setting. The assigned masks are written to mask_mapping.txt, so you can view what is being used. If the generator runs out of bits to use for masks, it will print an error and abort. In this case, you should inspect the mask_mappings, and decide which messages are not being sent as much, but have small masks. You should assign the mask_bits of these messages to pack_minimum_plus_8. This will make the mask use an extra byte, so if it previously filled the last 6 bits of a byte now fill 14 bits. (6 bits of the last, plus 8 bits more)

        b. dataTimeout: how long before telem dashboard raises a warning for not recieving this packet from vitals

        c. msgFields: This defines the fixed header fields of the packet. you can specify how many bits each field should be. You can also specify any field as an enum to send enums. The enum must be found inside enum.def for the codeGen to use.

        d. containsPayload: some messages dont parse cleanly, or what is attached might depend on previous parts of the msg, which can require manual parsing. In this case, use containsPayload= true. Then, in addition to the header defined by your msgFields, you may also attach a payload. The sender will need to tell the send function how many bytes of payload to use, and pass a payload byte array. The receiving side will need custom code to parse this payload. If you declare a payload, the other side must have custom implemented code to parse it, so that it knows how many bytes to skip until the next message within the Lora payload

    Information from packetFormat.py for vitals<->telem communication placed in .java LUTs in the presentation directory for the telem-dashboard, and .c LUTs inside vitalsGen for vitals. 

2. BMSParse: Used to log can packets comingout of the 2Q BMS in standalone mode. Parses the output from the espListen environment which logs every CAN message. Used for testing, not a part of final car.

3. telemetry_benchmakr: Used for benchmarking telemetry. Flash the telemRXBenchmark and telemTXBenchmark onto two esp's with Ebyte Lora module connected. These scripts parse the output of the telemRXEnvironment, and can create a plot Lora hw and protocol level stastics over time. Was used to observe how RSSI and throughput varied while walking with a telemTX microcontroller around campus