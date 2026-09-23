/*******************************************************************************
 * @file  gpio_uulp_example.c
 * @brief GPIO UULP example
 *******************************************************************************
 * # License
 * <b>Copyright 2022 Silicon Laboratories Inc. www.silabs.com</b>
 *******************************************************************************
 *
 * The licensor of this software is Silicon Laboratories Inc. Your use of this
 * software is governed by the terms of Silicon Labs Master Software License
 * Agreement (MSLA) available at
 * www.silabs.com/about-us/legal/master-software-license-agreement. This
 * software is distributed to you in Source Code format and is governed by the
 * sections of the MSLA applicable to Source Code.
 *
 ******************************************************************************/
/**============================================================================
 * @brief : This file contains example application for GPIO example
 * @section Description :
 * This application demonstrates the UULP pin interrupt, used here as the
 * manual SOS trigger button (BTN0).
 ============================================================================**/
#include "gpio_uulp_example.h"
#include "sl_si91x_driver_gpio.h"
#include "sl_gpio_board.h"
#include "rsi_debug.h"
#include "sl_sleeptimer.h"
#include <stdbool.h>

/*******************************************************************************
 ***************************  Defines / Macros  ********************************
 ******************************************************************************/
#define UULP_GPIO_INTR_2 UULP_GPIO_INTERRUPT_2
#define AVL_INTR_NO      0

#define SOS_DEBOUNCE_MS  300

/*******************************************************************************
 *************************** LOCAL VARIABLES   *********************************
 ******************************************************************************/
static sl_si91x_gpio_pin_config_t sl_gpio_pin_config1 = { { SL_SI91X_UULP_GPIO_2_PORT, SL_SI91X_UULP_GPIO_2_PIN },
                                                          GPIO_INPUT };

static volatile bool sos_button_pending   = false;
static uint32_t      sos_last_press_tick  = 0;

/*******************************************************************************
 **********************  Local Function prototypes   ***************************
 ******************************************************************************/
static void gpio_uulp_pin_interrupt_callback(uint32_t pin_intr);

/*******************************************************************************
 **************************   GLOBAL FUNCTIONS   *******************************
 ******************************************************************************/
void gpio_uulp_example_init(void)
{
  sl_status_t status;
  uulp_pad_config_t uulp_pad;
  do {
    uulp_pad.gpio_padnum = SL_SI91X_UULP_GPIO_2_PIN;
    uulp_pad.pad_select  = SET;
    uulp_pad.mode        = CLR;
    uulp_pad.direction   = SET;
    uulp_pad.receiver    = SET;

    status = sl_gpio_driver_init();
    if (status != SL_STATUS_OK) {
      DEBUGOUT("sl_gpio_driver_init, Error code: %lu\r\n", status);
      break;
    }
    DEBUGOUT("GPIO driver initialization is successful \r\n");
    status = sl_gpio_set_configuration(sl_gpio_pin_config1);
    if (status != SL_STATUS_OK) {
      DEBUGOUT("sl_gpio_set_configuration, Error code: %lu\r\n", status);
      break;
    }
    DEBUGOUT("GPIO driver set pin configuration is successful \r\n");
    status = sl_si91x_gpio_driver_set_uulp_pad_configuration(&uulp_pad);
    if (status != SL_STATUS_OK) {
      DEBUGOUT("sl_si91x_gpio_driver_set_uulp_pad_configuration, Error code: %lu\r\n", status);
      break;
    }
    DEBUGOUT("GPIO driver set uulp pad configuration is successful \r\n");
    status = sl_gpio_driver_configure_interrupt(&sl_gpio_pin_config1.port_pin,
                                                UULP_GPIO_INTR_2,
                                                (sl_gpio_interrupt_flag_t)SL_GPIO_INTERRUPT_RISE_EDGE,
                                                (sl_gpio_irq_callback_t)&gpio_uulp_pin_interrupt_callback,
                                                AVL_INTR_NO);
    if (status != SL_STATUS_OK) {
      DEBUGOUT("sl_gpio_configure_interrupt, Error code: %lu\r\n", status);
      break;
    }
    DEBUGOUT("GPIO driver configure uulp interrupt is successful \r\n");
  } while (false);
}

void gpio_uulp_example_process_action(void)
{
  if (sos_button_pending) {
    sos_button_pending = false;

    uint32_t now_tick   = sl_sleeptimer_get_tick_count();
    uint32_t elapsed_ms = sl_sleeptimer_tick_to_ms(now_tick - sos_last_press_tick);

    if (elapsed_ms >= SOS_DEBOUNCE_MS) {
      sos_last_press_tick = now_tick;
      DEBUGOUT("SOS BUTTON PRESSED\n");
      // TODO (Phase 6): call the same Firebase alert-send function used by
      // fall detection here, e.g. send_fall_alert(ALERT_TYPE_MANUAL_SOS);
    }
  }
}

static void gpio_uulp_pin_interrupt_callback(uint32_t pin_intr)
{
  if (pin_intr == UULP_GPIO_INTR_2) {
    sos_button_pending = true;
  }
}