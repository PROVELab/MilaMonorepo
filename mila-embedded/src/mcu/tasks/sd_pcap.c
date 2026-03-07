#include "sd_pcap.h"
#include "esp_timer.h"

static const char* TAG = "SDFMT";
static sdmmc_card_t* g_card; // must live for the life of the mount
static uint64_t g_start_time_us = 0;
static uint64_t g_last_record_time_us = 0;

static const uint32_t BUFFER_SIZE = 4096; // normal antics

static inline bool file_exists(const char* path) { return (path != NULL) && (access(path, F_OK) == 0); }

// Tori's Mount SD card thingy
esp_err_t sd_mount(const char* base_path, bool allow_format_if_needed) {
    esp_vfs_fat_sdmmc_mount_config_t mc = {.format_if_mount_failed =
                                               allow_format_if_needed, // true => auto-FAT32 if needed
                                           .max_files = 8,
                                           .allocation_unit_size = 16 * 1024};

    sdmmc_host_t host = SDMMC_HOST_DEFAULT();
    host.max_freq_khz = SDMMC_FREQ_PROBING; // 400 kHz for bring-up

    sdmmc_slot_config_t slot = SDMMC_SLOT_CONFIG_DEFAULT();
    slot.width = 1; // 1-bit: CLK14, CMD15, D0=2
    slot.gpio_cd = GPIO_NUM_NC;
    slot.gpio_wp = GPIO_NUM_NC;
    slot.flags |= SDMMC_SLOT_FLAG_INTERNAL_PULLUP;

    g_card = NULL; // mount will set this
    esp_err_t ret = esp_vfs_fat_sdmmc_mount(base_path, &host, &slot, &mc, &g_card);
    if (ret != ESP_OK) {
        ESP_LOGE("SD", "mount failed: %s", esp_err_to_name(ret));
        return ret;
    }

    sdmmc_card_print_info(stdout, g_card); // expects sdmmc_card_t*
    ESP_LOGI("SD", "mounted at %s", base_path);
    return ESP_OK;
}

// fsyncs
static inline void log_fsync(FILE* fp) {
    fflush(fp);
    fsync(fileno(fp)); // forces FAT directory entry (size/time) to be updated
}
// Writes the header to the file
static esp_err_t _pcap_write_header(FILE* fp, uint64_t start_time_us){
    if (!fp) return ESP_ERR_INVALID_ARG;

    pcap_header_s phs = DEFAULT_PCAP_HEADER_S;
    phs.start_time = start_time_us;

    // Write the header to the file
    if (fwrite(&phs, 1, sizeof(pcap_header_s), fp) != sizeof(pcap_header_s)) {
        ESP_LOGE(TAG, "Failed writing PCAP header");
        return ESP_FAIL;
    }
    return ESP_OK;
}

esp_err_t pcap_write_record(FILE * fp, CANPacket packet){
    if (!fp) return ESP_ERR_INVALID_ARG;
    if (packet.dataSize > sizeof(packet.data)) return ESP_ERR_INVALID_ARG;

    const uint64_t now_us = (uint64_t) esp_timer_get_time();
    uint64_t delta_us = 0;
    if (g_last_record_time_us > 0 && now_us >= g_last_record_time_us) {
        delta_us = now_us - g_last_record_time_us;
    }

    pcap_record_s prs = {0};
    prs.time_delta = (delta_us > UINT32_MAX) ? UINT32_MAX : (uint32_t) delta_us;
    prs.id = packet.id;
    prs.dlc_flags.dlc = packet.dataSize;
    prs.dlc_flags.flags = (packet.extendedID) ? 1 : 0;

    memcpy(prs.data, packet.data, packet.dataSize);
    prs.crc = j1850_compute(packet.data, packet.dataSize);
    g_last_record_time_us = now_us;

    // Write to file
    if (fwrite(&prs, 1, sizeof(pcap_record_s), fp) != sizeof(pcap_record_s)) {
        ESP_LOGE(TAG, "Failed writing PCAP record");
        return ESP_FAIL;
    }
    return ESP_OK;
}

// Creates first missing capX.pcap
// And opens it
esp_err_t pcap_start_logfile(log_file_t* out) {
    j1850_init_table(); // make sure crc'ing will work

    if (!out) return ESP_ERR_INVALID_ARG;
    out->fp = NULL;
    out->name[0] = '\0';

    for (int i = 0; i < 100; i++) {
        char path[64];
        snprintf(path, sizeof(path), "/sdcard/cap%d.pcap", i);
        if (!file_exists(path)) {
            FILE* fp = fopen(path, "wb"); // open up file in binary mode

            if (!fp) {
                ESP_LOGE(TAG, "Failed to create %s", path);
                return ESP_FAIL;
            }

            g_start_time_us = (uint64_t) esp_timer_get_time();
            g_last_record_time_us = g_start_time_us;

            esp_err_t ret = _pcap_write_header(fp, g_start_time_us); // Write the PCAP Header
            if (ret != ESP_OK) {
                fclose(fp);
                return ret;
            }
            log_fsync(fp); // Sync it up!

            setvbuf(fp, NULL, _IOFBF, BUFFER_SIZE); // normal buffering (4 Kb buffer)
            
            out->fp = fp;
            strncpy(out->name, path, sizeof(out->name) - 1);
            out->name[sizeof(out->name) - 1] = '\0';

            ESP_LOGI(TAG, "Created %s", out->name);
            return ESP_OK;
        }
    }
    return ESP_FAIL;
}
