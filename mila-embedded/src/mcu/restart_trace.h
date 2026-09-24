#ifndef MCU_RESTART_TRACE_H
#define MCU_RESTART_TRACE_H


// Consume the reset trace saved by the esp_restart() linker wrapper. `reset_reason`
// is returned separately because ROM reset codes and ESP-IDF's classification can
// occasionally disagree. The MCU emits its result through the VSR logger after
// logging has been initialized.
void log_restart_trace();

#endif
