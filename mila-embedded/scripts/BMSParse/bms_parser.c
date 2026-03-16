#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

// Struct definition using __attribute__((packed)) for GCC/Clang
typedef struct {
    // ID 49 (0x031)
    uint8_t  soc_raw;
    uint8_t  soh_raw;
    uint16_t pack_voltage_raw;
    int16_t  pack_current_raw;
    uint8_t  error_num;
    uint8_t  error_cell_temp;

    // ID 50 (0x032)
    int8_t   cell_temps[8];

    // ID 51-54 (0x033 - 0x036)
    uint16_t cell_voltages_raw[16];

    // ID 55 (0x037)
    uint16_t max_charge_voltage_raw;
    uint16_t max_charge_current_raw;
    uint16_t max_discharge_current_raw;
    uint16_t min_discharge_voltage_raw;

    // ID 56 (0x038)
    uint16_t ah;
    int8_t   bms_temp;
    uint8_t  io_status;
    uint16_t cell_balancing_status;
    uint8_t  contactor_status;
} __attribute__((packed)) BMSData;

// Global static instance as requested
static BMSData global_bms_data;
static uint8_t packet_flags = 0; // Bitmask to track IDs 49-56 (bits 0-7)

// State machine definitions
typedef enum {
    STATE_SEARCHING,
    STATE_READING_DATA
} ParserState;

// Function to map the 8 bytes into the global struct based on CAN ID
void parse_packet(int id, uint8_t *data) {
    int bit_index = id - 49;
    if (bit_index < 0 || bit_index > 7) return; // Out of bounds

    switch (id) {
        case 49: // 0x031
            global_bms_data.soc_raw = data[0];
            global_bms_data.soh_raw = data[1];
            global_bms_data.pack_voltage_raw = (data[2] << 8) | data[3];
            global_bms_data.pack_current_raw = (int16_t)((data[4] << 8) | data[5]);
            global_bms_data.error_num = data[6];
            global_bms_data.error_cell_temp = data[7];
            break;
        case 50: // 0x032
            for (int i = 0; i < 8; i++) {
                global_bms_data.cell_temps[i] = (int8_t)data[i];
            }
            break;
        case 51: // 0x033
            for (int i = 0; i < 4; i++) {
                global_bms_data.cell_voltages_raw[i] = (data[i * 2] << 8) | data[i * 2 + 1];
            }
            break;
        case 52: // 0x034
            for (int i = 0; i < 4; i++) {
                global_bms_data.cell_voltages_raw[i + 4] = (data[i * 2] << 8) | data[i * 2 + 1];
            }
            break;
        case 53: // 0x035
            for (int i = 0; i < 4; i++) {
                global_bms_data.cell_voltages_raw[i + 8] = (data[i * 2] << 8) | data[i * 2 + 1];
            }
            break;
        case 54: // 0x036
            for (int i = 0; i < 4; i++) {
                global_bms_data.cell_voltages_raw[i + 12] = (data[i * 2] << 8) | data[i * 2 + 1];
            }
            break;
        case 55: // 0x037
            global_bms_data.max_charge_voltage_raw = (data[0] << 8) | data[1];
            global_bms_data.max_charge_current_raw = (data[2] << 8) | data[3];
            global_bms_data.max_discharge_current_raw = (data[4] << 8) | data[5];
            global_bms_data.min_discharge_voltage_raw = (data[6] << 8) | data[7];
            break;
        case 56: // 0x038
            global_bms_data.ah = (data[0] << 8) | data[1];
            global_bms_data.bms_temp = (int8_t)data[2];
            global_bms_data.io_status = data[3];
            global_bms_data.cell_balancing_status = (data[4] << 8) | data[5];
            global_bms_data.contactor_status = data[7]; // Byte 7 (index 6) is unused
            break;
    }

    // Mark this packet ID as received
    packet_flags |= (1 << bit_index);
}

// Write the fully collected global struct out to the CSV
void log_csv_row(FILE *csv, uint32_t timestamp) {
    fprintf(csv, "%u,", timestamp);
    
    // Apply conversions per datasheet
    fprintf(csv, "%.1f,%.1f,%.2f,%.2f,%u,%u,", 
            global_bms_data.soc_raw * 0.5,
            global_bms_data.soh_raw * 0.5,
            global_bms_data.pack_voltage_raw * 0.01,
            global_bms_data.pack_current_raw * 0.02,
            global_bms_data.error_num,
            global_bms_data.error_cell_temp);

    // Temp 1-8
    for (int i = 0; i < 8; i++) {
        fprintf(csv, "%d,", global_bms_data.cell_temps[i]);
    }

    // Cell 1-16 Voltages (Convert mV to V)
    for (int i = 0; i < 16; i++) {
        fprintf(csv, "%.3f,", global_bms_data.cell_voltages_raw[i] * 0.001);
    }

    // Limits & Stats
    fprintf(csv, "%.1f,%.1f,%.1f,%.1f,%u,%d,%u,0x%04X,%u\n",
            global_bms_data.max_charge_voltage_raw * 0.1,
            global_bms_data.max_charge_current_raw * 0.1,
            global_bms_data.max_discharge_current_raw * 0.1,
            global_bms_data.min_discharge_voltage_raw * 0.1,
            global_bms_data.ah,
            global_bms_data.bms_temp,
            global_bms_data.io_status,
            global_bms_data.cell_balancing_status,
            global_bms_data.contactor_status);
}

int main() {
    FILE *infile = fopen("BMSSCANLOG.txt", "r");
    if (!infile) {
        printf("Error: Could not open BMSSCANLOG.txt\n");
        return 1;
    }

    FILE *outfile = fopen("BMS_Parsed.csv", "w");
    if (!outfile) {
        printf("Error: Could not create output CSV file.\n");
        fclose(infile);
        return 1;
    }

    // Write CSV Header
    fprintf(outfile, "Timestamp_ms,SOC_%%,SOH_%%,Pack_V,Pack_A,ErrorNum,ErrorCellTemp,");
    for(int i=1; i<=8; i++) fprintf(outfile, "Temp%d_C,", i);
    for(int i=1; i<=16; i++) fprintf(outfile, "Cell%d_V,", i);
    fprintf(outfile, "MaxCharge_V,MaxCharge_A,MaxDischarge_A,MinDischarge_V,Ah,BMS_Temp_C,IO_Status,Balancing_Status_Hex,Contactor_Status\n");

    char line[256];
    ParserState state = STATE_SEARCHING;
    
    // Variables for relative timestamps
    uint32_t first_timestamp = 0;
    int first_timestamp_set = 0;
    uint32_t current_timestamp = 0;
    
    int current_id = 0;
    uint8_t data_buf[8];
    int data_idx = 0;

    while (fgets(line, sizeof(line), infile)) {
        if (state == STATE_SEARCHING) {
            // Check for Timestamp
            char *ts_ptr = strstr(line, "I (");
            if (ts_ptr) {
                uint32_t raw_ts;
                sscanf(ts_ptr, "I (%u)", &raw_ts);
                
                // If this is the very first timestamp we've seen, save it as the baseline
                if (!first_timestamp_set) {
                    first_timestamp = raw_ts;
                    first_timestamp_set = 1;
                }
                
                // Calculate the relative timestamp
                current_timestamp = raw_ts - first_timestamp;
            }
            // Check for ID
            else if (strstr(line, "id ")) {
                sscanf(strstr(line, "id "), "id %d", &current_id);
            }
            // Check for start of data payload
            else if (strstr(line, "with data:")) {
                state = STATE_READING_DATA;
                data_idx = 0;
            }
        } 
        else if (state == STATE_READING_DATA) {
            unsigned int hex_val;
            // Read lines until 8 valid hex bytes are found
            if (sscanf(line, "%x", &hex_val) == 1) {
                data_buf[data_idx++] = (uint8_t)hex_val;
                
                if (data_idx == 8) {
                    parse_packet(current_id, data_buf);
                    state = STATE_SEARCHING;

                    // If all 8 message IDs have been received, write row and reset flags
                    if (packet_flags == 0xFF) {
                        log_csv_row(outfile, current_timestamp);
                        fflush(outfile); 
                        packet_flags = 0;
                    }
                }
            }
        }
    }

    fclose(infile);
    fclose(outfile);
    printf("Parsing complete. Output saved to BMS_Parsed.csv\n");

    return 0;
}