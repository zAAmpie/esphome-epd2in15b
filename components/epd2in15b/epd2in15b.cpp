#include "epd2in15b.h"
#include "esphome/core/log.h"
#include "esphome/core/application.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

namespace esphome {
namespace epd2in15b {

static const char *const TAG = "epd2in15b";

// ── Low-level SPI helpers ─────────────────────────────────────────────────────

void EPD2in15B::send_command_(uint8_t cmd) {
  this->dc_pin_->digital_write(false);  // DC LOW = command
  this->enable();
  this->write_byte(cmd);
  this->disable();
}

void EPD2in15B::send_data_(uint8_t data) {
  this->dc_pin_->digital_write(true);   // DC HIGH = data
  this->enable();
  this->write_byte(data);
  this->disable();
}

// ── Hardware control ──────────────────────────────────────────────────────────

void EPD2in15B::reset_() {
  if (this->reset_pin_ != nullptr) {
    this->reset_pin_->digital_write(true);
    delay(200);
    this->reset_pin_->digital_write(false);
    delay(2);
    this->reset_pin_->digital_write(true);
    delay(200);
  }
}

void EPD2in15B::wait_until_idle_() {
  ESP_LOGD(TAG, "Waiting for display idle...");
  vTaskDelay(50 / portTICK_PERIOD_MS);
  uint32_t start = millis();
  while (this->busy_pin_ != nullptr && this->busy_pin_->digital_read()) {
    if (millis() - start > 30000) {
      ESP_LOGE(TAG, "Timeout waiting for display idle!");
      return;
    }
    App.feed_wdt();
    vTaskDelay(10 / portTICK_PERIOD_MS);
  }
  vTaskDelay(50 / portTICK_PERIOD_MS);
  ESP_LOGD(TAG, "Display idle.");
}

void EPD2in15B::set_window_(uint16_t x_start, uint16_t y_start, uint16_t x_end, uint16_t y_end) {
  this->send_command_(0x44);  // SET_RAM_X_ADDRESS_START_END_POSITION
  this->send_data_((x_start >> 3) & 0x1F);
  this->send_data_((x_end >> 3) & 0x1F);

  this->send_command_(0x45);  // SET_RAM_Y_ADDRESS_START_END_POSITION
  this->send_data_(y_start & 0xFF);
  this->send_data_((y_start >> 8) & 0x01);
  this->send_data_(y_end & 0xFF);
  this->send_data_((y_end >> 8) & 0x01);
}

void EPD2in15B::set_cursor_(uint16_t x, uint16_t y) {
  this->send_command_(0x4E);  // SET_RAM_X_ADDRESS_COUNTER
  this->send_data_(x & 0x1F);

  this->send_command_(0x4F);  // SET_RAM_Y_ADDRESS_COUNTER
  this->send_data_(y & 0xFF);
  this->send_data_((y >> 8) & 0x01);
}

void EPD2in15B::turn_on_display_() {
  ESP_LOGD(TAG, "Triggering display refresh (0x20)...");
  this->send_command_(0x20);  // MASTER_ACTIVATION
  this->wait_until_idle_();
  ESP_LOGD(TAG, "Display refresh complete.");
}

// ── Initialisation ────────────────────────────────────────────────────────────

void EPD2in15B::initialize_() {
  this->reset_();
  this->wait_until_idle_();

  this->send_command_(0x12);  // SWRESET
  this->wait_until_idle_();

  this->send_command_(0x11);  // Data entry mode: X increment, Y increment
  this->send_data_(0x03);

  this->set_window_(0, 0, EPD_WIDTH - 1, EPD_HEIGHT - 1);

  this->send_command_(0x3C);  // Border waveform
  this->send_data_(0x05);

  this->send_command_(0x18);  // Read built-in temperature sensor
  this->send_data_(0x80);

  // RAM content option for display update — enable both black and red RAM
  this->send_command_(0x21);
  this->send_data_(0x00);
  this->send_data_(0x80);

  this->set_cursor_(0, 0);
  this->wait_until_idle_();
}

void EPD2in15B::clear_() {
  // Send all-white black plane
  this->send_command_(0x24);
  this->dc_pin_->digital_write(true);
  this->enable();
  for (uint32_t i = 0; i < EPD_BLACK_BUFFER_SIZE; i++) {
    this->write_byte(0xFF);
    if (i % 256 == 0) App.feed_wdt();
  }
  this->disable();

  // Send all-white red plane (0x00 stored = 0xFF sent = no red)
  this->send_command_(0x26);
  this->dc_pin_->digital_write(true);
  this->enable();
  for (uint32_t i = 0; i < EPD_RED_BUFFER_SIZE; i++) {
    this->write_byte(0x00);
    if (i % 256 == 0) App.feed_wdt();
  }
  this->disable();

  this->turn_on_display_();
}

// ── ESPHome lifecycle ─────────────────────────────────────────────────────────

void EPD2in15B::setup() {
  // Allocate the display buffer first — must happen before any rendering
  // Use a single allocation split into two planes
  this->init_internal_(EPD_BLACK_BUFFER_SIZE + EPD_RED_BUFFER_SIZE);
  if (this->buffer_ == nullptr) {
    ESP_LOGE(TAG, "Failed to allocate display buffer!");
    this->mark_failed();
    return;
  }
  this->black_buffer_ = this->buffer_;
  this->red_buffer_   = this->buffer_ + EPD_BLACK_BUFFER_SIZE;

  // White background: black plane all 1s (white), red plane all 0s (no red)
  // Red plane is inverted on send: stored 0x00 → sent 0xFF → no red pixels
  memset(this->black_buffer_, 0xFF, EPD_BLACK_BUFFER_SIZE);
  memset(this->red_buffer_,   0x00, EPD_RED_BUFFER_SIZE);

  // Set up pins
  this->dc_pin_->setup();
  this->dc_pin_->digital_write(false);

  // Power on display first — must happen before SPI init
  if (this->pwr_pin_ != nullptr) {
    this->pwr_pin_->setup();
    this->pwr_pin_->digital_write(true);
    delay(100);  // Allow display to stabilise after power on
  }

  if (this->reset_pin_ != nullptr) {
    this->reset_pin_->setup();
    this->reset_pin_->digital_write(true);
  }

  if (this->busy_pin_ != nullptr) {
    this->busy_pin_->setup();
  }

  this->spi_setup();
  this->initialize_();
}

void EPD2in15B::dump_config() {
  LOG_DISPLAY("", "Waveshare 2.15\" B ePaper", this);
  ESP_LOGCONFIG(TAG, "  Width: %d, Height: %d", EPD_WIDTH, EPD_HEIGHT);
  LOG_PIN("  DC Pin: ", this->dc_pin_);
  LOG_PIN("  Reset Pin: ", this->reset_pin_);
  LOG_PIN("  Busy Pin: ", this->busy_pin_);
}

// ── Pixel drawing ─────────────────────────────────────────────────────────────

void EPD2in15B::draw_absolute_pixel_internal(int x, int y, Color color) {
  if (this->black_buffer_ == nullptr || this->red_buffer_ == nullptr)
    return;
  if (x < 0 || x >= EPD_WIDTH || y < 0 || y >= EPD_HEIGHT)
    return;

  // Each byte holds 8 pixels, MSB first
  uint32_t byte_idx = (x / 8) + y * (EPD_WIDTH / 8);
  uint8_t  bit_mask = 0x80 >> (x % 8);

  // Black plane: 0 = black pixel, 1 = white pixel
  // Red plane:   1 = red pixel,   0 = not red
  // (note: red plane is inverted before sending, so we store uninverted here)

  if (color.r > 200 && color.g < 100 && color.b < 100) {
    // Red pixel: set red plane, clear black plane (white)
    this->black_buffer_[byte_idx] |= bit_mask;   // white in black plane
    this->red_buffer_[byte_idx]   |= bit_mask;   // red in red plane
  } else if (color.r < 50 && color.g < 50 && color.b < 50) {
    // Black pixel
    this->black_buffer_[byte_idx] &= ~bit_mask;  // black in black plane
    this->red_buffer_[byte_idx]   &= ~bit_mask;  // not red
  } else {
    // White pixel (or anything else)
    this->black_buffer_[byte_idx] |= bit_mask;   // white in black plane
    this->red_buffer_[byte_idx]   &= ~bit_mask;  // not red
  }
}

// ── Display update ────────────────────────────────────────────────────────────

void EPD2in15B::update() {
  ESP_LOGD(TAG, "update() called, running lambda...");

  // Reset buffers to white before running lambda
  memset(this->black_buffer_, 0xFF, EPD_BLACK_BUFFER_SIZE);  // all white
  memset(this->red_buffer_,   0x00, EPD_RED_BUFFER_SIZE);    // no red
  // Re-run lambda to fill framebuffers
  this->do_update_();
  ESP_LOGD(TAG, "lambda done, sending buffers...");
  ESP_LOGD(TAG, "black[0]=0x%02X red[0]=0x%02X", this->black_buffer_[0], this->red_buffer_[0]);

  ESP_LOGD(TAG, "Sending black plane...");
  // Reset cursor to top-left before sending frame data
  this->set_cursor_(0, 0);

  // Send black plane (0x24): 0=black, 1=white
  this->send_command_(0x24);
  this->dc_pin_->digital_write(true);
  this->enable();
  for (uint32_t i = 0; i < EPD_BLACK_BUFFER_SIZE; i++) {
    this->write_byte(this->black_buffer_[i]);
    if (i % 256 == 0) App.feed_wdt();
  }
  this->disable();

  // Send red plane (0x26): inverted on send (0=red, 1=not red)
  this->send_command_(0x26);
  this->dc_pin_->digital_write(true);
  this->enable();
  for (uint32_t i = 0; i < EPD_RED_BUFFER_SIZE; i++) {
    this->write_byte(~this->red_buffer_[i]);
    if (i % 256 == 0) App.feed_wdt();
  }
  this->disable();

  this->turn_on_display_();
}

}  // namespace epd2in15b
}  // namespace esphome