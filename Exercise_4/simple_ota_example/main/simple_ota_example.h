/*
 * simple_ota_example.h
 *
 *  Created on: 11 Jul 2019
 *      Author: besho
 */

#ifndef MAIN_SIMPLE_OTA_EXAMPLE_H_
#define MAIN_SIMPLE_OTA_EXAMPLE_H_

#include <stdint.h>
#include <esp_err.h>
#include <esp_http_client.h>

extern const uint8_t server_cert_pem_start[] asm("_binary_ca_cert_pem_start");
extern const uint8_t server_cert_pem_end[] asm("_binary_ca_cert_pem_end");
esp_err_t _http_event_handler(esp_http_client_event_t *evt);

#endif /* MAIN_SIMPLE_OTA_EXAMPLE_H_ */
