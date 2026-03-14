#pragma once

#include "esphome/core/component.h"
#include "esphome/components/display/display_buffer.h"
#include "esphome/components/spi/spi.h"

namespace esphome {
namespace epd2in15b {

static const uint16_t EPD_WIDTH  = 160;
static const uint16_t EPD_HEIGHT = 296;
static const uint32_t EPD_BLACK_BUFFER_SIZE = (EPD_WIDTH / 8) * EPD_HEIGHT;
static const uint32_t EPD_RED_BUFFER_SIZE   = (EPD_WIDTH / 8) * EPD_HEIGHT;

enum class EPDState : uint8_t {
  IDLE,
  INIT_WAIT_BUSY,
  INIT_SWRESET,
  INIT_SWRESET_WAIT,
  INIT_REGISTERS,
  UPDATE_SEND_DATA,
  UPDATE_ACTIVATE,
  UPDATE_WAIT_BUSY,
};

class EPD2in15B : public display::DisplayBuffer,
                  public spi::SPIDevice<spi::BIT_ORDER_MSB_FIRST,
                                        spi::CLOCK_POLARITY_LOW,
                                        spi::CLOCK_PHASE_LEADING,
                                        spi::DATA_RATE_8MHZ> {
 public:
  void set_dc_pin(GPIOPin *dc)     { dc_pin_ = dc; }
  void set_reset_pin(GPIOPin *rst) { reset_pin_ = rst; }
  void set_busy_pin(GPIOPin *busy) { busy_pin_ = busy; }
  void set_pwr_pin(GPIOPin *pwr)   { pwr_pin_ = pwr; }

  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::PROCESSOR; }

  display::DisplayType get_display_type() override { return display::DisplayType::DISPLAY_TYPE_COLOR; }
  void update() override;
  int get_width_internal()  override { return EPD_WIDTH; }
  int get_height_internal() override { return EPD_HEIGHT; }

 protected:
  uint8_t *black_buffer_{nullptr};
  uint8_t *red_buffer_{nullptr};

  GPIOPin *dc_pin_{nullptr};
  GPIOPin *reset_pin_{nullptr};
  GPIOPin *busy_pin_{nullptr};
  GPIOPin *pwr_pin_{nullptr};

  EPDState state_{EPDState::IDLE};
  uint32_t state_deadline_ms_{0};
  bool update_pending_{false};

  void draw_absolute_pixel_internal(int x, int y, Color color) override;
  size_t get_buffer_length_() { return EPD_BLACK_BUFFER_SIZE + EPD_RED_BUFFER_SIZE; }

  void send_command_(uint8_t cmd);
  void send_data_(uint8_t data);
  void set_window_(uint16_t x_start, uint16_t y_start, uint16_t x_end, uint16_t y_end);
  void set_cursor_(uint16_t x, uint16_t y);
  bool is_busy_();
  void transition_(EPDState next, uint32_t delay_ms = 0);
  void do_reset_();
};

}  // namespace epd2in15b
}  // namespace esphome