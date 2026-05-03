#include "tasks.h"
#include "vsr_uart_shared.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_log.h"

#include <pb_decode.h>
#include <stdint.h>
#include <string.h>

#define MOTOR_COMMAND_PAYLOAD_MAX_LEN    256
#define MOTOR_COMMAND_STREAM_CHUNK_LEN   128
#define MOTOR_COMMAND_STREAM_BUFFER_LEN  (8 * (VSR_UART_FRAME_HEADER_LEN + MOTOR_COMMAND_PAYLOAD_MAX_LEN))
#define MOTOR_COMMAND_RX_POLL_TIMEOUT_MS 2

static bool motor_command_has_valid_mode(const vsr_MotorCommand* command) {
    if (command == NULL || !command->has_command) { return false; }

    switch (command->command.which_kind) {
        case vsr_MotorCommand_CommandValue_park_tag:
        case vsr_MotorCommand_CommandValue_reverse_tag:
        case vsr_MotorCommand_CommandValue_neutral_tag:
        case vsr_MotorCommand_CommandValue_drive_tag:
        case vsr_MotorCommand_CommandValue_cruise_control_tag: return true;
        default: return false;
    }
}

static const char* motor_command_mode_name(const vsr_MotorCommand* command) {
    if (command == NULL || !command->has_command) {
        return "UNKNOWN";
    }

    switch (command->command.which_kind) {
        case vsr_MotorCommand_CommandValue_park_tag: return "PARK";
        case vsr_MotorCommand_CommandValue_reverse_tag: return "REVERSE";
        case vsr_MotorCommand_CommandValue_neutral_tag: return "NEUTRAL";
        case vsr_MotorCommand_CommandValue_drive_tag: return "DRIVE";
        case vsr_MotorCommand_CommandValue_cruise_control_tag: return "CRUISE_CONTROL";
        default: return "UNKNOWN";
    }
}

static bool decode_motor_command(const uint8_t* payload, size_t payload_len, vsr_MotorCommand* out_command) {
    if (payload == NULL || out_command == NULL || payload_len == 0 || payload_len > MOTOR_COMMAND_PAYLOAD_MAX_LEN) {
        return false;
    }

    const vsr_MotorCommand zero = vsr_MotorCommand_init_zero;
    *out_command = zero;
    pb_istream_t stream = pb_istream_from_buffer(payload, payload_len);
    if (!pb_decode(&stream, vsr_MotorCommand_fields, out_command)) { return false; }

    return motor_command_has_valid_mode(out_command);
}

static void update_vsr_motor_command(const vsr_MotorCommand* command) {
    volatile vehicle_status_reg_t* vsr = &vsr_global;
    ACQ_REL_VSRSEM_W(vsr, motor_command, {
        VSR_DATA.motor_command = *command;
        VSR_DATA.motor_command.has_command = true;
    });
}

static void handle_motor_command_payload(const uint8_t* payload, size_t payload_len) {
    vsr_MotorCommand command = vsr_MotorCommand_init_zero;
    if (!decode_motor_command(payload, payload_len, &command)) {
        ESP_LOGE(__func__, "Error decoding motor command payload (len=%u)", (unsigned) payload_len);
        return;
    }
    update_vsr_motor_command(&command);
    ESP_LOGI(__func__, "Applied motor command: %s", motor_command_mode_name(&command));
}

static void drain_motor_command_frames(uint8_t* stream_buf, size_t* stream_len) {
    static uint32_t invalid_len_drops = 0;
    size_t cursor = 0;

    while (cursor + VSR_UART_FRAME_HEADER_LEN <= *stream_len) {
        if (stream_buf[cursor] != VSR_UART_FRAME_MAGIC_0 || stream_buf[cursor + 1] != VSR_UART_FRAME_MAGIC_1) {
            cursor += 1;
            continue;
        }

        size_t payload_len = (size_t) stream_buf[cursor + 2] | ((size_t) stream_buf[cursor + 3] << 8);
        if (payload_len == 0 || payload_len > MOTOR_COMMAND_PAYLOAD_MAX_LEN) {
            // Invalid length for a motor command payload, advance a byte and resync.
            invalid_len_drops++;
            if ((invalid_len_drops % 200) == 0) {
                ESP_LOGW(__func__, "dropping invalid motor command frame length=%u (drops=%lu)", (unsigned) payload_len,
                         (unsigned long) invalid_len_drops);
            }
            cursor += 1;
            continue;
        }

        size_t frame_len = VSR_UART_FRAME_HEADER_LEN + payload_len;
        if (cursor + frame_len > *stream_len) { break; }

        handle_motor_command_payload(&stream_buf[cursor + VSR_UART_FRAME_HEADER_LEN], payload_len);
        cursor += frame_len;
    }

    if (cursor > 0) {
        memmove(stream_buf, stream_buf + cursor, *stream_len - cursor);
        *stream_len -= cursor;
    }
}

static void motor_command_rx_main(void* arg) {
    (void) arg;
    ESP_LOGI(__func__, "motor command RX task started on UART%u", (unsigned) VSR_UART_NUM);

    uint8_t rx_chunk[MOTOR_COMMAND_STREAM_CHUNK_LEN];
    uint8_t stream_buf[MOTOR_COMMAND_STREAM_BUFFER_LEN];
    size_t stream_len = 0;

    while (1) {
        int bytes_read =
            uart_read_bytes(VSR_UART_NUM, rx_chunk, sizeof(rx_chunk), pdMS_TO_TICKS(MOTOR_COMMAND_RX_POLL_TIMEOUT_MS));
        if (bytes_read <= 0) { continue; }

        vsr_append_stream_bytes(stream_buf, &stream_len, sizeof(stream_buf), rx_chunk, (size_t) bytes_read);
        drain_motor_command_frames(stream_buf, &stream_len);
    }
}

void start_motor_command_rx_task() {
    static StackType_t motor_command_rx_stack[DEFAULT_STACK_SIZE];
    static StaticTask_t motor_command_rx_tcb;

    xTaskCreateStaticPinnedToCore(motor_command_rx_main, "motor_command_rx", DEFAULT_STACK_SIZE, NULL,
                                  MOTOR_COMMAND_RX_TASK_PRIO, motor_command_rx_stack, &motor_command_rx_tcb, 0);
}
