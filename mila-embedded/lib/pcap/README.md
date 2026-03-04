# PCAP

PCAP is a simple logging format to store
raw CAN packets that flow throw a 2.0A/B network.

A single node can essentially MITM the network and capture/trace
every single message that passes by, and log it to PCAP. PCAP files
can then be decoded in a protocol-specific way as part of postprocessing
and data analysis.

## PCAP Log Format ##

The full log format is simple, consisting of a single fixed-size
header and an unlimited number of fixed-size records of which each
is a CAN packet that was logged:

| Full Log Format |
|-----------------|
| Header          |
| Record*         |

#### PCAP Header ####

| Field    | magic                          | start_timestamp                                        |
|----------|--------------------------------|--------------------------------------------------------|
| Size (b) | 4                              | 8                                                      |
| Notes    | Magic. Should always be 'MILA' | The time when logging started (in terms of node start) |


#### PCAP Body ####

| Field    | id                  | flags                                                                   | dlc                                               | data     | crc                       |
|----------|---------------------|-------------------------------------------------------------------------|---------------------------------------------------|----------|---------------------------|
| Size (b) | 4                   | 4 bits                                                                  | 4 bits                                            | 8 bytes  | 1                         |
| Notes    | The 11 or 29-bit ID | Flags. The only flag supported right now is EXD in which case flags = 1 | How many bytes of data is actually in this record | The data | SAE J1850 CRC8 (checksum) |

