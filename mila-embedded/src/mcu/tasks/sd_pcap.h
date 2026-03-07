#ifndef SD_PCAP_H
#define SD_PCAP_H

#include "pcap.h"
#include "pecan/pecan.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/fcntl.h>
#include <sys/unistd.h>
#include <unistd.h>

#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "diskio_impl.h" // for ff_diskio_register_sdmmc (ESP-IDF 5.3.x)
#include "driver/sdmmc_host.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"


typedef struct {
    FILE* fp;
    char name[48];
} log_file_t;


esp_err_t pcap_write_record(FILE * fp, CANPacket packet);
esp_err_t pcap_start_logfile(log_file_t* out);

#endif
