#include "LEDShow.h"

#include "driver/rmt_tx.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"

// TM1934 control input, after a 3.3 V -> 5 V buffer/level shifter.
constexpr gpio_num_t DATA_PIN = GPIO_NUM_13;

// 20 MHz gives 50 ns timing steps, allowing an exact 1.25 us TM1934 bit cell.
constexpr uint32_t RMT_RESOLUTION_HZ = 20000000;
constexpr uint16_t TM1934_T0H_TICKS = 7;
constexpr uint16_t TM1934_T0L_TICKS = 18;
constexpr uint16_t TM1934_T1H_TICKS = 14;
constexpr uint16_t TM1934_T1L_TICKS = 11;
constexpr uint16_t TM1934_RESET_US = 250;

static_assert(sizeof(CRGB) == 3, "CRGB must map directly to TM1934 RGB bytes");

CRGB leds[NUM_LEDS];
LEDShowController LEDShow;

static CRGB scaled_leds[NUM_LEDS];

static const rmt_symbol_word_t tm1934_zero = {
    TM1934_T0H_TICKS, 1, TM1934_T0L_TICKS, 0,
};

static const rmt_symbol_word_t tm1934_one = {
    TM1934_T1H_TICKS, 1, TM1934_T1L_TICKS, 0,
};

static size_t tm1934_encode(const void *data, size_t data_size,
                            size_t symbols_written, size_t symbols_free,
                            rmt_symbol_word_t *symbols, bool *done, void *)
{
    if (symbols_free < 8) {
        return 0;
    }

    const size_t byte_index = symbols_written / 8;
    if (byte_index < data_size) {
        const uint8_t byte = static_cast<const uint8_t *>(data)[byte_index];
        for (uint8_t mask = 0x80, i = 0; mask != 0; mask >>= 1, ++i) {
            symbols[i] = (byte & mask) ? tm1934_one : tm1934_zero;
        }
        return 8;
    }

    const uint16_t half_reset_ticks =
        (RMT_RESOLUTION_HZ / 1000000 * TM1934_RESET_US) / 2;
    symbols[0] = {half_reset_ticks, 0, half_reset_ticks, 0};
    *done = true;
    return 1;
}

void LEDShowController::begin()
{
    rmt_tx_channel_config_t channel_config = {};
    channel_config.gpio_num = DATA_PIN;
    channel_config.clk_src = RMT_CLK_SRC_DEFAULT;
    channel_config.resolution_hz = RMT_RESOLUTION_HZ;
    channel_config.mem_block_symbols = 64;
    channel_config.trans_queue_depth = 1;

    rmt_channel_handle_t channel = nullptr;
    ESP_ERROR_CHECK(rmt_new_tx_channel(&channel_config, &channel));

    rmt_simple_encoder_config_t encoder_config = {};
    encoder_config.callback = tm1934_encode;

    rmt_encoder_handle_t encoder = nullptr;
    ESP_ERROR_CHECK(rmt_new_simple_encoder(&encoder_config, &encoder));
    ESP_ERROR_CHECK(rmt_enable(channel));

    channel_ = channel;
    encoder_ = encoder;
}

void LEDShowController::setBrightness(uint8_t brightness)
{
    brightness_ = brightness;
}

uint8_t LEDShowController::getBrightness() const
{
    return brightness_;
}

void LEDShowController::show()
{
    // Do not scale leds[] in-place. Changing brightness later will rescale the
    // original configured RGB values rather than compounding prior scaling.
    for (uint16_t i = 0; i < NUM_LEDS; ++i) {
        scaled_leds[i] = {
            static_cast<uint8_t>((static_cast<uint16_t>(leds[i].r) * brightness_ + 127) / 255),
            static_cast<uint8_t>((static_cast<uint16_t>(leds[i].g) * brightness_ + 127) / 255),
            static_cast<uint8_t>((static_cast<uint16_t>(leds[i].b) * brightness_ + 127) / 255),
        };
    }

    rmt_transmit_config_t transmit_config = {};
    ESP_ERROR_CHECK(rmt_transmit(static_cast<rmt_channel_handle_t>(channel_),
                                 static_cast<rmt_encoder_handle_t>(encoder_),
                                 scaled_leds, sizeof(scaled_leds),
                                 &transmit_config));
    ESP_ERROR_CHECK(rmt_tx_wait_all_done(static_cast<rmt_channel_handle_t>(channel_),
                                         portMAX_DELAY));
}
