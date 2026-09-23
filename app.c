/***************************************************************************/ /**
 * @file
 * @brief Fall Detection + SOS Firebase Application
 *******************************************************************************
 * # License
 * <b>Copyright 2022 Silicon Laboratories Inc. www.silabs.com</b>
 *******************************************************************************
 *
 * SPDX-License-Identifier: Zlib
 ******************************************************************************/
#include "cmsis_os2.h"
#include "sl_net.h"
#include "sl_board_configuration.h"
#include "sl_net_ping.h"
#include "sl_utility.h"
#include "sl_si91x_driver.h"
#include "sl_wifi.h"
#include "sl_net_wifi_types.h"
#include "sl_net_default_values.h"
#include "icm40627_example.h"
#include "gpio_uulp_example.h"
#include "rsi_debug.h"
#include <string.h>

#define CONNECT_WITH_PMK 0
#define MAIN_LOOP_DELAY_MS 20

const osThreadAttr_t thread_attributes = {
  .name       = "app",
  .attr_bits  = 0,
  .cb_mem     = 0,
  .cb_size    = 0,
  .stack_mem  = 0,
  .stack_size = 3072,
  .priority   = osPriorityLow,
  .tz_module  = 0,
  .reserved   = 0,
};

static void application_start(void *argument);
static sl_status_t network_event_handler(sl_net_event_t event, sl_status_t status, void *data, uint32_t data_length);

void app_init(void)
{
  osThreadNew((osThreadFunc_t)application_start, NULL, &thread_attributes);
}

static void application_start(void *argument)
{
  UNUSED_PARAMETER(argument);
  sl_status_t status;

  // Sensors and button init first, independent of Wi-Fi
  icm40627_example_init();
  gpio_uulp_example_init();

  status = sl_net_init(SL_NET_WIFI_CLIENT_INTERFACE, NULL, NULL, network_event_handler);
  if (status != SL_STATUS_OK) {
    printf("\r\nFailed to start Wi-Fi Client interface: 0x%lX\r\n", status);
    return;
  }
  printf("\r\nWi-Fi client interface up success\r\n");

#if CONNECT_WITH_PMK
  uint8_t pairwise_master_key[32] = { 0 };
  sl_wifi_ssid_t ssid;
  uint8_t type = 3;
  ssid.length  = (uint8_t)(sizeof(DEFAULT_WIFI_CLIENT_PROFILE_SSID) - 1);
  memcpy(ssid.value, DEFAULT_WIFI_CLIENT_PROFILE_SSID, ssid.length);

  status = sl_wifi_get_pairwise_master_key(SL_WIFI_CLIENT_INTERFACE, type, &ssid,
                                           DEFAULT_WIFI_CLIENT_CREDENTIAL, pairwise_master_key);
  if (status != SL_STATUS_OK) { printf("\r\nGet Pairwise Master Key Failed: 0x%lX\r\n", status); return; }

  status = sl_net_set_profile(SL_NET_WIFI_CLIENT_INTERFACE, SL_NET_DEFAULT_WIFI_CLIENT_PROFILE_ID,
                              &DEFAULT_WIFI_CLIENT_PROFILE);
  if (status != SL_STATUS_OK) { printf("\r\nFailed to set client profile: 0x%lx\r\n", status); return; }

  status = sl_net_set_credential(SL_NET_DEFAULT_WIFI_CLIENT_CREDENTIAL_ID, SL_NET_WIFI_PMK,
                                 pairwise_master_key, sizeof(pairwise_master_key));
  if (status != SL_STATUS_OK) { printf("\r\nFailed sl_net_set_credential: 0x%lX\r\n", status); return; }
#endif

  status = sl_net_up(SL_NET_WIFI_CLIENT_INTERFACE, SL_NET_DEFAULT_WIFI_CLIENT_PROFILE_ID);
  if (status != SL_STATUS_OK) {
    printf("\r\nFailed to bring Wi-Fi client interface up: 0x%lX\r\n", status);
    // Non-fatal: fall detection and SOS button must keep working even if Wi-Fi fails.
  } else {
    printf("\r\nWi-Fi client connected\r\n");
  }

  DEBUGOUT("Entering main loop\n");
  uint32_t loop_counter = 0;
  while (1) {
    loop_counter++;
    if (loop_counter % 50 == 0) {
      DEBUGOUT("Main loop alive, counter=%lu\n", loop_counter);
    }
    icm40627_example_process_action();
    gpio_uulp_example_process_action();
    osDelay(MAIN_LOOP_DELAY_MS);
  }
}

static sl_status_t network_event_handler(sl_net_event_t event, sl_status_t status, void *data, uint32_t data_length)
{
  UNUSED_PARAMETER(data_length);
  switch (event) {
    case SL_NET_PING_RESPONSE_EVENT: {
      sl_net_ping_response_t *response = (sl_net_ping_response_t *)data;
      if (status != SL_STATUS_OK) { printf("\r\nPing request failed!\r\n"); return status; }
      printf("\r\n%u bytes received from %u.%u.%u.%u\r\n",
             response->ping_size,
             response->ping_address.ipv4_address[0], response->ping_address.ipv4_address[1],
             response->ping_address.ipv4_address[2], response->ping_address.ipv4_address[3]);
      break;
    }
    case SL_NET_DHCP_NOTIFICATION_EVENT:
      printf("\r\nReceived DHCP Notification event with status : 0x%lX\r\n", status);
      break;
    case SL_NET_IP_ADDRESS_CHANGE_EVENT: {
      sl_net_ip_configuration_t *ip_config = (sl_net_ip_configuration_t *)data;
      printf("\r\nReceived Ip Address Change Notification event with status : 0x%lX\r\n", status);
      printf("\t Ip Address : %u.%u.%u.%u\r\n",
             ip_config->ip.v4.ip_address.bytes[0], ip_config->ip.v4.ip_address.bytes[1],
             ip_config->ip.v4.ip_address.bytes[2], ip_config->ip.v4.ip_address.bytes[3]);
      break;
    }
    default:
      break;
  }
  return SL_STATUS_OK;
}