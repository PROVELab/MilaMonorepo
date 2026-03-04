#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#include "pcap/pcap.h"

#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "../vsr.h"
#include "tasks.h" // defines LOGGING_TASK_PRIO (fallback provided below if missing)

#include "driver/sdmmc_host.h"
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"

#ifndef LOGGING_TASK_PRIO
#define LOGGING_TASK_PRIO 5
#endif

#include "diskio_impl.h" // for ff_diskio_register_sdmmc (ESP-IDF 5.3.x)
#include "driver/sdmmc_host.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"

#include <sys/fcntl.h>
#include <sys/unistd.h>
#include <unistd.h>

static const char* TAG = "SDFMT";
static sdmmc_card_t* g_card; // must live for the life of the mount

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

static inline void log_fsync(FILE* fp) {
    fflush(fp);
    fsync(fileno(fp)); // forces FAT directory entry (size/time) to be updated
}

typedef struct {
    FILE* fp;
    char name[48];
} log_file_t;

// Writes the header to the file
static esp_err_t _pcap_write_header(FILE* fp){
    pcap_header_s phs;
   
    // Write the header to the file
    write(fp, (void*) &phs, sizeof(pcap_header_s));
}

esp_err_t pcap_write_record(FILE * fp, CANPacket packet){
    pcap_record_s prs = {
        .id = packet.id,
        .dlc_flags.dlc = packet.dataSize,
        .dlc_flags.flags = (packet.extendedID) ? 1 : 0,
        .crc = 0
    };

    memcpy(prs.data, packet.data, packet.dataSize);
    prs.crc = j1850_compute(packet.data, packet.dataSize);

    // Write to file
    write(fp, &prs, sizeof(pcap_record_s));
}

// Creates first missing capX.pcap
// And opens it
static esp_err_t pcap_create_next(log_file_t* out) {
    if (!out) return ESP_ERR_INVALID_ARG;
    out->fp = NULL;
    out->name[0] = '\0';

    for (int i = 0; i < 100; i++) {
        char path[64];
        snprintf(path, sizeof(path), "/sdcard/cap%d.pcap", i);
        if (!file_exists(path)) {
            FILE* fp = fopen(path, "w");
            if (!fp) {
                ESP_LOGE(TAG, "Failed to create %s", path);
                return ESP_FAIL;
            }
            csv_header(fp);
            log_fsync(fp);
            setvbuf(fp, NULL, _IOLBF, 0); // line-buffered
            out->fp = fp;
            strncpy(out->name, path, sizeof(out->name) - 1);
            out->name[sizeof(out->name) - 1] = '\0';
            ESP_LOGI(TAG, "Created %s", out->name);
            return ESP_OK;
        }
    }
    return ESP_FAIL;
}
