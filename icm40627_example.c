/***************************************************************************/ /**
 * @file icm40627_example.c
 * @brief ICM40627 example APIs
 *******************************************************************************
 * # License
 * <b>Copyright 2024 Silicon Laboratories Inc. www.silabs.com</b>
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
#include "rsi_debug.h"
#include "sl_si91x_icm40627.h"
#include "icm40627_example.h"
#include "sl_sleeptimer.h"
#include "sl_si91x_ssi.h"
#include "rsi_rom_clks.h"
#include "sl_status.h"
#include "sl_si91x_driver_gpio.h"
#include <math.h>
#include <stdbool.h>

/*******************************************************************************
 ***************************  Defines / Macros  ********************************
 ******************************************************************************/
#define DELAY_PERIODIC_MS1 20 //sleeptimer1 periodic timeout in ms

// --- Fall detection tunables (values confirmed working via real drop tests) ---
#define FALL_SAMPLES_PER_SEC       50     // matches 20ms periodic timer
#define FALL_FREEFALL_THRESHOLD    0.6f   // g - magnitude below this = possible free-fall
#define FALL_IMPACT_THRESHOLD      2.0f   // g - magnitude above this = possible impact
#define FALL_STILLNESS_LOW         0.7f   // g - lower bound of "settled/still" band
#define FALL_STILLNESS_HIGH        1.3f   // g - upper bound of "settled/still" band

#define FALL_FREEFALL_MIN_SAMPLES     3                          // ~60ms minimum free-fall dip
#define FALL_IMPACT_WINDOW_SAMPLES    (1 * FALL_SAMPLES_PER_SEC) // must see impact within 1s of free-fall
#define FALL_STILLNESS_WINDOW_SAMPLES (2 * FALL_SAMPLES_PER_SEC) // must stay still ~2s after impact
#define FALL_STILLNESS_TOLERANCE_SAMPLES  15  // ~300ms of sustained motion allowed before cancelling

// Set to 1 while tuning fall detection, 0 for silent final behavior
#define FALL_DEBUG_MODE 1

/*******************************************************************************
 ******************************  Data Types  ***********************************
 ******************************************************************************/
typedef enum {
  FALL_STATE_NORMAL,
  FALL_STATE_FREEFALL_DETECTED,
  FALL_STATE_IMPACT_DETECTED,
  FALL_STATE_CONFIRMED
} fall_state_t;

/*******************************************************************************
 *************************** LOCAL VARIABLES   *******************************
 ******************************************************************************/
sl_sleeptimer_timer_handle_t timer1;
boolean_t delay_timeout;
static sl_ssi_handle_t ssi_driver_handle = NULL;
static uint32_t ssi_slave_number         = SSI_SLAVE_0;

static fall_state_t fall_state         = FALL_STATE_NORMAL;
static int fall_freefall_count         = 0;
static int fall_window_counter         = 0;
static int fall_outofband_streak       = 0;

/*******************************************************************************
 **********************  Local Function prototypes   ***************************
 ******************************************************************************/
static void on_timeout_timer1(sl_sleeptimer_timer_handle_t *handle, void *data);
static sl_status_t enable_icm40627(bool connect);
static bool fall_detection_update(float ax, float ay, float az);

/*******************************************************************************
 **************************   GLOBAL FUNCTIONS   *******************************
 ******************************************************************************/
void icm40627_example_init(void)
{
  sl_status_t sl_status;
  uint8_t dev_id;

  do {
    sl_status = enable_icm40627(true);
    if (sl_status != SL_STATUS_OK) {
      DEBUGOUT("ICM40627 enable failed, Error Code: 0x%ld \n", sl_status);
      break;
    } else {
      DEBUGOUT("ICM40627 enable successful\n");
    }

    sl_status = sl_si91x_icm40627_ssi_interface_init(&ssi_driver_handle, ssi_slave_number);
    if (sl_status != SL_STATUS_OK) {
      DEBUGOUT("ICM40627 SSI interface init failed, Error Code: 0x%ld \n", sl_status);
      break;
    } else {
      DEBUGOUT("ICM40627 SSI interface init successful\n");
    }

    sl_status_t timer_status = sl_sleeptimer_start_periodic_timer_ms(&timer1,
                                          DELAY_PERIODIC_MS1,
                                          on_timeout_timer1,
                                          NULL,
                                          0,
                                          SL_SLEEPTIMER_NO_HIGH_PRECISION_HF_CLOCKS_REQUIRED_FLAG);
    DEBUGOUT("Sleeptimer start status: 0x%lx\n", timer_status);

    sl_status = sl_si91x_icm40627_software_reset(ssi_driver_handle);
    if (sl_status != SL_STATUS_OK) {
      DEBUGOUT("ICM40627 software reset un-successful, Error Code: 0x%ld \n", sl_status);
      break;
    } else {
      DEBUGOUT("ICM40627 software reset successful\n");
    }

    sl_status = sl_si91x_icm40627_get_device_id(ssi_driver_handle, &dev_id);
    if ((sl_status == SL_STATUS_OK) && (dev_id == ICM40627_DEVICE_ID)) {
      DEBUGOUT("ICM40627 device ID verification successful \n");
    } else {
      DEBUGOUT("ICM40627 device ID verification failed\n");
      break;
    }

    sl_status = sl_si91x_icm40627_init(ssi_driver_handle);
    if (sl_status != SL_STATUS_OK) {
      DEBUGOUT("ICM40627 initialization failed, Error Code: 0x%ld \n", sl_status);
      break;
    } else {
      DEBUGOUT("ICM40627 initialization successful\n");
    }
  } while (false);
}

void icm40627_example_process_action(void)
{
  sl_status_t status;
  float temperature = 0;
  float sensor_data[3];
  float accel_x = 0, accel_y = 0, accel_z = 0;

  if (delay_timeout == true) {
    delay_timeout = false;

    status = sl_si91x_icm40627_get_temperature_data(ssi_driver_handle, &temperature);
    if (status != SL_STATUS_OK) {
      DEBUGOUT("Temperature read failed, Error Code: 0x%ld \n", status);
    }

    status = sl_si91x_icm40627_get_accel_data(ssi_driver_handle, sensor_data);
    if (status != SL_STATUS_OK) {
      DEBUGOUT("Acceleration read failed, Error Code: 0x%ld \n", status);
    } else {
      accel_x = sensor_data[0];
      accel_y = sensor_data[1];
      accel_z = sensor_data[2];

#if FALL_DEBUG_MODE
      float mag = sqrtf(accel_x * accel_x + accel_y * accel_y + accel_z * accel_z);
      DEBUGOUT("mag=%0.2f state=%d\n", mag, (int)fall_state);
#endif

      if (fall_detection_update(accel_x, accel_y, accel_z)) {
        DEBUGOUT("FALL DETECTED\n");
        // TODO (Phase 6): call the Firebase alert-send function here, e.g.
        // send_fall_alert();
      }
    }

    status = sl_si91x_icm40627_get_gyro_data(ssi_driver_handle, sensor_data);
    if (status != SL_STATUS_OK) {
      DEBUGOUT("Gyro read failed, Error Code: 0x%ld \n", status);
    }
  }
}

void icm40627_fall_detection_reset(void)
{
  fall_state            = FALL_STATE_NORMAL;
  fall_freefall_count   = 0;
  fall_window_counter   = 0;
  fall_outofband_streak = 0;
}

static void on_timeout_timer1(sl_sleeptimer_timer_handle_t *handle, void *data)
{
  (void)&handle;
  (void)&data;
  delay_timeout = true;
}

static bool fall_detection_update(float ax, float ay, float az)
{
  float magnitude = sqrtf(ax * ax + ay * ay + az * az);

  switch (fall_state) {

    case FALL_STATE_NORMAL:
      if (magnitude < FALL_FREEFALL_THRESHOLD) {
        fall_freefall_count++;
        if (fall_freefall_count >= FALL_FREEFALL_MIN_SAMPLES) {
          fall_state = FALL_STATE_FREEFALL_DETECTED;
          fall_window_counter = 0;
        }
      } else {
        fall_freefall_count = 0;
      }
      break;

    case FALL_STATE_FREEFALL_DETECTED:
      fall_window_counter++;
      if (magnitude > FALL_IMPACT_THRESHOLD) {
        fall_state = FALL_STATE_IMPACT_DETECTED;
        fall_window_counter = 0;
        fall_outofband_streak = 0;
      } else if (fall_window_counter > FALL_IMPACT_WINDOW_SAMPLES) {
        fall_state = FALL_STATE_NORMAL;
        fall_freefall_count = 0;
      }
      break;

    case FALL_STATE_IMPACT_DETECTED:
      fall_window_counter++;
      if (magnitude >= FALL_STILLNESS_LOW && magnitude <= FALL_STILLNESS_HIGH) {
        fall_outofband_streak = 0;
        if (fall_window_counter >= FALL_STILLNESS_WINDOW_SAMPLES) {
          fall_state = FALL_STATE_CONFIRMED;
          return true;
        }
      } else {
        fall_outofband_streak++;
        if (fall_outofband_streak >= FALL_STILLNESS_TOLERANCE_SAMPLES) {
          fall_state = FALL_STATE_NORMAL;
          fall_freefall_count = 0;
          fall_outofband_streak = 0;
        }
      }
      break;

    case FALL_STATE_CONFIRMED:
      break;
  }

  return false;
}

static sl_status_t enable_icm40627(bool connect)
{
  sl_status_t status;
  if (sl_si91x_gpio_driver_get_uulp_npss_pin(SENSOR_ENABLE_GPIO_PIN) != 1) {
    status = sl_si91x_gpio_driver_enable_clock((sl_si91x_gpio_select_clock_t)ULPCLK_GPIO);
    if (status != SL_STATUS_OK) {
      return status;
    }

    if (connect) {
      status = sl_si91x_gpio_driver_set_uulp_npss_pin_mux(SENSOR_ENABLE_GPIO_PIN, NPSS_GPIO_PIN_MUX_MODE1);
      if (status != SL_STATUS_OK) {
        return status;
      }
      status =
        sl_si91x_gpio_driver_set_uulp_npss_direction(SENSOR_ENABLE_GPIO_PIN, (sl_si91x_gpio_direction_t)GPIO_OUTPUT);
      if (status != SL_STATUS_OK) {
        return status;
      }
      status = sl_si91x_gpio_driver_set_uulp_npss_pin_value(SENSOR_ENABLE_GPIO_PIN, SET);
      if (status != SL_STATUS_OK) {
        return status;
      }
    } else {
      status = sl_si91x_gpio_driver_set_uulp_npss_pin_value(SENSOR_ENABLE_GPIO_PIN, CLR);
      if (status != SL_STATUS_OK) {
        return status;
      }
    }
  }
  return SL_STATUS_OK;
}