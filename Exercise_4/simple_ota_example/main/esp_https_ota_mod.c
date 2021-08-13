// Copyright 2017-2018 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at

//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "esp_https_ota_mod.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <esp_ota_ops.h>
#include <esp_log.h>
#include "simple_ota_example.h"
#include "mbedtls/base64.h"
#include "mbedtls/pk.h"
#include "mbedtls/sha256.h"

#define OTA_BUF_SIZE 256
static const char *TAG = "esp_https_ota";

static void http_cleanup(esp_http_client_handle_t client) {
	esp_http_client_close(client);
	esp_http_client_cleanup(client);
}

esp_err_t esp_https_ota_mod(const esp_http_client_config_t *config) {
	if (!config) {
		ESP_LOGE(TAG, "esp_http_client config not found");
		return ESP_ERR_INVALID_ARG;
	}

	if (!config->cert_pem && !config->use_global_ca_store) {
		ESP_LOGE(TAG,
				"Server certificate not found, either through configuration or global CA store");
		return ESP_ERR_INVALID_ARG;
	}

	esp_http_client_handle_t client = esp_http_client_init(config);
	if (client == NULL) {
		ESP_LOGE(TAG, "Failed to initialise HTTP connection");
		return ESP_FAIL;
	}

	if (esp_http_client_get_transport_type(client) != HTTP_TRANSPORT_OVER_SSL) {
		ESP_LOGE(TAG, "Transport is not over HTTPS");
		return ESP_FAIL;
	}

	esp_err_t err = esp_http_client_open(client, 0);
	if (err != ESP_OK) {
		esp_http_client_cleanup(client);
		ESP_LOGE(TAG, "Failed to open HTTP connection: %s",
				esp_err_to_name(err));
		return err;
	}
	esp_http_client_fetch_headers(client);
	esp_ota_handle_t update_handle = 0;
	const esp_partition_t *update_partition = NULL;
	ESP_LOGI(TAG, "Starting OTA...");
	update_partition = esp_ota_get_next_update_partition(NULL);
	if (update_partition == NULL) {
		ESP_LOGE(TAG, "Passive OTA partition not found");
		http_cleanup(client);
		return ESP_FAIL;
	}
	ESP_LOGI(TAG, "Writing to partition subtype %d at offset 0x%x",
			update_partition->subtype, update_partition->address);

	err = esp_ota_begin(update_partition, OTA_SIZE_UNKNOWN, &update_handle);
	if (err != ESP_OK) {
		ESP_LOGE(TAG, "esp_ota_begin failed, error=%d", err);
		http_cleanup(client);
		return err;
	}
	ESP_LOGI(TAG, "esp_ota_begin succeeded");
	ESP_LOGI(TAG, "Please Wait. This may take time");

	esp_err_t ota_write_err = ESP_OK;
	char *upgrade_data_buf = (char *) malloc(OTA_BUF_SIZE);
	if (!upgrade_data_buf) {
		ESP_LOGE(TAG, "Couldn't allocate memory to upgrade data buffer");
		http_cleanup(client);
		return ESP_ERR_NO_MEM;
	}
	// Init SHA-256
	unsigned char fw_hash[32];
	mbedtls_sha256_context sha_ctx;
	mbedtls_sha256_init(&sha_ctx);
	mbedtls_sha256_starts(&sha_ctx, 0); /* SHA-256, not 224 */

	int binary_file_len = 0;
	while (1) {
		int data_read = esp_http_client_read(client, upgrade_data_buf,
				OTA_BUF_SIZE);
		if (data_read == 0) {
			ESP_LOGI(TAG, "Connection closed,all data received");
			break;
		}
		if (data_read < 0) {
			ESP_LOGE(TAG, "Error: SSL data read error");
			http_cleanup(client);
			free(upgrade_data_buf);
			return -1;
		}
		if (data_read > 0) {
			mbedtls_sha256_update(&sha_ctx,
					(const unsigned char*) upgrade_data_buf, data_read);
			ota_write_err = esp_ota_write(update_handle,
					(const void *) upgrade_data_buf, data_read);
			if (ota_write_err != ESP_OK) {
				free(upgrade_data_buf);
				http_cleanup(client);
				return ota_write_err;
			}
			binary_file_len += data_read;
			ESP_LOGD(TAG, "Written image length %d", binary_file_len);
		}
	}
	mbedtls_sha256_finish(&sha_ctx, fw_hash);
	mbedtls_sha256_free(&sha_ctx);

	printf("Calculated FW Hash: ");
	for (int i = 0; i < sizeof(fw_hash); i++) {
		printf("%02x", fw_hash[i]);
	}
	printf("\r\n");

	free(upgrade_data_buf);
	http_cleanup(client);
	ESP_LOGD(TAG, "Total binary data length writen: %d", binary_file_len);

	// Download signature
	esp_http_client_config_t config_sig = {
			.url = "https://www.emsec2019.pi/b64sign.sign",
			.cert_pem =	(char *) server_cert_pem_start,
			.event_handler = _http_event_handler
	};
	esp_http_client_handle_t client_sig = esp_http_client_init(&config_sig);
	if (client_sig == NULL) {
		ESP_LOGE(TAG, "SIG Failed to initialise HTTP connection");
		return ESP_FAIL;
	}
	if (esp_http_client_get_transport_type(client_sig)
			!= HTTP_TRANSPORT_OVER_SSL) {
		ESP_LOGE(TAG, "SIG Transport is not over HTTPS");
		return ESP_FAIL;
	}

	esp_err_t err_sig = esp_http_client_open(client_sig, 0);
	if (err_sig != ESP_OK) {
		esp_http_client_cleanup(client_sig);
		ESP_LOGE(TAG, "SIG Failed to open HTTP connection: %s",
				esp_err_to_name(err_sig));
		return err_sig;
	}
	esp_http_client_fetch_headers(client_sig);
	char sig_data_buf[344];
	int data_read_sig = 0;
	do {
		data_read_sig = esp_http_client_read(client_sig,
				sig_data_buf + data_read_sig,
				sizeof(sig_data_buf) - data_read_sig);
		if (data_read_sig == 0) {
			ESP_LOGI(TAG, "SIG Connection closed,all data received");
		}
		if (data_read_sig < 0) {
			ESP_LOGE(TAG, "SIG Error: SSL data read error");
			http_cleanup(client_sig);
			return -1;
		}
	} while (data_read_sig > 0);
	printf("Signature: ");
	for (int i = 0; i < sizeof(sig_data_buf); i++) {
		printf("%c", sig_data_buf[i]);
	}
	printf("\r\n");
	http_cleanup(client_sig);

	// Decode Base64 Signature
	unsigned char dec_sig[256];
	size_t olen;
	int dec_ret = mbedtls_base64_decode(dec_sig, sizeof(dec_sig), &olen,
			(const unsigned char*) sig_data_buf, sizeof(sig_data_buf));
	if (dec_ret == 0) {
		printf("Decoded Signature: ");
		for (int i = 0; i < sizeof(dec_sig); i++) {
			printf("%02X", dec_sig[i]);
		}
		printf("\r\n");
	} else {
		printf("Failed to decode signature with error 0x%x.\r\n", dec_ret);
		return dec_ret;
	}

	// Parse Key
	const unsigned char pub_key[] = "-----BEGIN PUBLIC KEY-----\n"
			"MIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEApUtrcngVVQJxB8LDqhoh\n"
			"2sH7O1QDWsgNHho7Y4FNi7/d2a1w9spx+3rOvrhdA9iOihXvFwBqlv626Bwn0wKy\n"
			"ibeTSfhizc/OhKIAZm9EKpdbdH8SgWYdL7hxiTRcNGcIb3i3FCBD3Jik1Y5od+fx\n"
			"1jMT4btZLqCy+xPNEfYhziQ+PqEU7HwhC/yUpjK6svgdDMuJD79mr8i6TRSzuaQz\n"
			"AKZ4ekV+LshPoxXCMyUN1GvXXnOK76ttjjtPaN2NB2WDlghZfvI3oe1qZBrCMvOy\n"
			"+pjjO3jSavtGzTvaXBsq0zj/QpbChHMUvbwhw6O/zq5d+QI0APxrX92j4FMc/e2N\n"
			"EQIDAQAB\n"
			"-----END PUBLIC KEY-----";
	mbedtls_pk_context pk_ctx;
	mbedtls_pk_init(&pk_ctx);
	int pk_parse_ret = mbedtls_pk_parse_public_key(&pk_ctx, pub_key,
			sizeof(pub_key));
	if (pk_parse_ret == 0) {
		printf("Successfully parsed public key!\r\n");
	} else {
		printf("Failed to parse public key with error 0x%x!\r\n", pk_parse_ret);
		return pk_parse_ret;
	}

	// Verify Signature
	int ver_ret = mbedtls_pk_verify(&pk_ctx, MBEDTLS_MD_SHA256,
			(const unsigned char*) fw_hash, sizeof(fw_hash),
			(const unsigned char*) dec_sig, sizeof(dec_sig));
	if (ver_ret == 0) {
		printf("Signature verified!\r\n");
	} else {
		printf("Invalid signature. Error 0x%x.\r\n", ver_ret);
		return ver_ret;
	}

	esp_err_t ota_end_err = esp_ota_end(update_handle);
	if (ota_write_err != ESP_OK) {
		ESP_LOGE(TAG, "Error: esp_ota_write failed! err=0x%d", err);
		return ota_write_err;
	} else if (ota_end_err != ESP_OK) {
		ESP_LOGE(TAG, "Error: esp_ota_end failed! err=0x%d. Image is invalid",
				ota_end_err);
		return ota_end_err;
	}

	err = esp_ota_set_boot_partition(update_partition);
	if (err != ESP_OK) {
		ESP_LOGE(TAG, "esp_ota_set_boot_partition failed! err=0x%d", err);
		return err;
	}
	ESP_LOGI(TAG, "esp_ota_set_boot_partition succeeded");

	return ESP_OK;
}
