import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import pins
from esphome.components import display, spi
from esphome.const import (
    CONF_DC_PIN,
    CONF_ID,
    CONF_LAMBDA,
    CONF_RESET_PIN,
    CONF_BUSY_PIN,
)

CONF_PWR_PIN = "pwr_pin"

DEPENDENCIES = ["spi"]
AUTO_LOAD = ["display"]

epd2in15b_ns = cg.esphome_ns.namespace("epd2in15b")
EPD2in15B = epd2in15b_ns.class_(
    "EPD2in15B",
    display.DisplayBuffer,
    spi.SPIDevice,
)

# Top-level component schema — referenced as:
#   epd2in15b:
#     id: my_display
#     ...
CONFIG_SCHEMA = cv.All(
    display.FULL_DISPLAY_SCHEMA.extend(
        {
            cv.GenerateID(): cv.declare_id(EPD2in15B),
            cv.Required(CONF_DC_PIN): pins.gpio_output_pin_schema,
            cv.Optional(CONF_RESET_PIN): pins.gpio_output_pin_schema,
            cv.Optional(CONF_BUSY_PIN): pins.gpio_input_pin_schema,
            cv.Optional(CONF_PWR_PIN): pins.gpio_output_pin_schema,
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(spi.spi_device_schema(cs_pin_required=True))
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await display.register_display(var, config)
    await spi.register_spi_device(var, config)

    dc_pin = await cg.gpio_pin_expression(config[CONF_DC_PIN])
    cg.add(var.set_dc_pin(dc_pin))

    if CONF_RESET_PIN in config:
        reset_pin = await cg.gpio_pin_expression(config[CONF_RESET_PIN])
        cg.add(var.set_reset_pin(reset_pin))

    if CONF_BUSY_PIN in config:
        busy_pin = await cg.gpio_pin_expression(config[CONF_BUSY_PIN])
        cg.add(var.set_busy_pin(busy_pin))

    if CONF_PWR_PIN in config:
        pwr_pin = await cg.gpio_pin_expression(config[CONF_PWR_PIN])
        cg.add(var.set_pwr_pin(pwr_pin))

    if CONF_LAMBDA in config:
        lambda_ = await cg.process_lambda(
            config[CONF_LAMBDA],
            [(display.DisplayRef, "it")],
            return_type=cg.void,
        )
        cg.add(var.set_writer(lambda_))