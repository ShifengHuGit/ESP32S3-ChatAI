/*
   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <stdio.h>
#include <stdlib.h>
#include <sys/param.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_process_sdkconfig.h"
#include "esp_wn_iface.h"
#include "esp_wn_models.h"
#include "esp_afe_sr_iface.h"
#include "esp_afe_sr_models.h"
#include "esp_mn_iface.h"
#include "esp_mn_models.h"
#include "esp_board_init.h"
#include "driver/i2s.h"

#include "model_path.h"


#include "esp_system.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_http_client.h"
#include "esp_tls.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "lwip/err.h"
#include "lwip/sys.h"

#include "wav_encoder.h"
#include "esp_spiffs.h"
#include "httpService.h"
#include "openai.h"
#include "wifi_api.h"
#include "Secert.h"



extern const char root_cert_pem_start[] asm("_binary_ocicert_pem_start");
extern const char azureroot_cert_pem_start[] asm("_binary_azurecert_pem_start");
extern const char azuresttroot_cert_pem_start[] asm("_binary_azuresttcert_pem_start");

/* The event group allows multiple bits for each event, but we only care about two events:
 * - we are connected to the AP with an IP
 * - we failed to connect after the maximum amount of retries */
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT BIT1

static const char *TAG = "wifi station";
static const char *FileTAG = "File System";

int detect_flag = 0;
static esp_afe_sr_iface_t *afe_handle = NULL;
static esp_afe_sr_data_t *afe_data = NULL;
static volatile int task_flag = 0;
srmodel_list_t *models = NULL;
static int play_voice = -2;

#define WAVE_MAX_SIZE 184364


struct wav_encoder *ww;
TaskHandle_t taskHandle;


void print_dot(void *pvParameters)
{
    while (1)
    {
        //printf(".");
        vTaskDelay(1000 / portTICK_PERIOD_MS); // 每秒打印一个点
    }
}

void wavHeader(char *header, int wavSize)
{
    header[0] = 'R';
    header[1] = 'I';
    header[2] = 'F';
    header[3] = 'F';
    unsigned int fileSize = wavSize + 44 - 8;
    header[4] = (char)(fileSize & 0xFF);
    header[5] = (char)((fileSize >> 8) & 0xFF);
    header[6] = (char)((fileSize >> 16) & 0xFF);
    header[7] = (char)((fileSize >> 24) & 0xFF);
    header[8] = 'W';
    header[9] = 'A';
    header[10] = 'V';
    header[11] = 'E';
    header[12] = 'f';
    header[13] = 'm';
    header[14] = 't';
    header[15] = ' ';

    header[16] = 0x10;
    header[17] = 0x00;
    header[18] = 0x00;
    header[19] = 0x00;
    // Format
    header[20] = 0x01;
    header[21] = 0x00;
    // Channel
    header[22] = 0x01;
    header[23] = 0x00;
    // Sample Rate 16000
    header[24] = 0x80;
    header[25] = 0x3E;
    header[26] = 0x00;
    header[27] = 0x00;
    // Bits per Sample
    header[28] = 0x00;
    header[29] = 0x7D;
    header[30] = 0x00;
    header[31] = 0x00;

    header[32] = 0x02;
    header[33] = 0x00;

    header[34] = 0x10;
    header[35] = 0x00;

    header[36] = 'd';
    header[37] = 'a';
    header[38] = 't';
    header[39] = 'a';
    header[40] = (char)(wavSize & 0xFF);
    header[41] = (char)((wavSize >> 8) & 0xFF);
    header[42] = (char)((wavSize >> 16) & 0xFF);
    header[43] = (char)((wavSize >> 24) & 0xFF);
}
void feed_Task(void *arg)
{
    esp_afe_sr_data_t *afe_data = arg;
    int audio_chunksize = afe_handle->get_feed_chunksize(afe_data);
    // int nch = afe_handle->get_channel_num(afe_data);
    int feed_channel = esp_get_feed_channel();
    int16_t *i2s_buff = malloc(audio_chunksize * sizeof(int16_t) * feed_channel);
    assert(i2s_buff);
    // size_t bytes_read;

    while (task_flag)
    {
        esp_get_feed_data(i2s_buff, audio_chunksize * sizeof(int16_t) * feed_channel);

        afe_handle->feed(afe_data, i2s_buff);
    }
    if (i2s_buff)
    {
        free(i2s_buff);
        i2s_buff = NULL;
    }
    vTaskDelete(NULL);
}

void detect_Task(void *arg)
{


    HttpTaskParas AzureSpeech2Text;
    HttpTaskParas AzureToken;

    int recording_flag = 0;
    int offset = 7;

    char AureToken_buffer[AZURE_TOKEN_MAX_LEN];
    char *AuthPrefix = "Bearer ";

    esp_afe_sr_data_t *afe_data = arg;

    //-----HTTP Initial----
    esp_http_client_config_t httpconfig = {
        .url = OCIUrl,
        .transport_type = HTTP_TRANSPORT_OVER_SSL,
        .event_handler = _http_event_handler,
        .cert_pem = root_cert_pem_start,
    };

    esp_http_client_config_t AzureSTTconfig = {
        .url = AzureSTTEndPointURL,
        .transport_type = HTTP_TRANSPORT_OVER_SSL,
        .event_handler = _Azure_Speech_event_handler,
        .buffer_size_tx = 2048,
        .cert_pem = azuresttroot_cert_pem_start,
    };
    //------
    esp_http_client_config_t AzureTokenconfig = {
        .url = AzureTokenURL,
        .transport_type = HTTP_TRANSPORT_OVER_SSL,
        .event_handler = _CommonHttpCpyResp,
        .cert_pem = azureroot_cert_pem_start,
        .user_data = AureToken_buffer + offset,
    };

AzureToken.httpCfg = AzureTokenconfig;
AzureToken.payload = NULL;
AzureToken.payload_len = "0";
AzureToken.Method = HTTP_METHOD_POST;
AzureToken.ContentType = "audio/wave";

HttpGetAzureToken(&AzureToken, AZURE_KEY);
memcpy(AureToken_buffer, AuthPrefix, offset);
AureToken_buffer[AZURE_TOKEN_MAX_LEN-1] = '\0';



AzureSpeech2Text.httpCfg = AzureSTTconfig;
AzureSpeech2Text.Method = HTTP_METHOD_POST;
AzureSpeech2Text.Auth = AureToken_buffer;
AzureSpeech2Text.ContentType = "audio/wave";

printf("-----------detect start------------\n");

while (task_flag)
{

    afe_fetch_result_t *res = afe_handle->fetch(afe_data);
    if (!res || res->ret_value == ESP_FAIL)
    {
        printf("fetch error!\n");
        break;
    }

    if (res->wakeup_state == WAKENET_DETECTED)
    {
        ESP_LOGE("Alexa", "---->我在.----");
        // play_voice = -1;
        detect_flag = 1;
        afe_handle->disable_wakenet(afe_data);
    }

    if (detect_flag == 1)
    {
        if (!recording_flag)
        {
            ww = wav_encoder_open("/spiffs/recording.wav", 16000, 16, 1);
            ESP_LOGI("Alexa", "---->我正在听---");
            xTaskCreate(print_dot, "printDot", 1024, NULL, 1, &taskHandle);
        }
        if (ww->data_length < 184364)
        {
            recording_flag = 1;
            wav_encoder_run(ww, (unsigned char *)res->data, res->data_size);
            continue;
        }
        else
        {
            vTaskDelete(taskHandle);
            wav_encoder_close(ww);
            recording_flag = 0;

            // ESP_LOGI("-HTTP-", "Uploading / Converting to Text.   Wav: %dKB", file_size);
            FILE *wavfile = fopen("/spiffs/recording.wav", "r");
            fseek(wavfile, 0L, SEEK_END);
            int file_size = ftell(wavfile);
            printf("\n");
            ESP_LOGI("Alexa", "---->RECORDED---");

            fseek(wavfile, 0L, SEEK_SET);

            char *waveFilebuffer = (char *)malloc(file_size + 1);
            fread(waveFilebuffer, 1, file_size, wavfile);
            waveFilebuffer[file_size] = '\0';
            fclose(wavfile);

            // Upload to OCI to verify the voice recording correct or not.
            // https_with_hostname_path(httpconfig, waveFilebuffer, file_size);
            
            char str_len[10];
            sprintf(str_len, "%d", file_size);
            AzureSpeech2Text.payload_len = str_len;
            AzureSpeech2Text.payload = waveFilebuffer;

            xTaskCreatePinnedToCore(&HttpAzureSpeech2Text, "Azure speech to text task", 16 * 1024, (void *)&AzureSpeech2Text, 5, NULL, 1);

            // Send to Azure do the Speech to Text
            // https_with_hostname_path(AzureSTTconfig, waveFilebuffer, file_size);
            // continue;

            afe_handle->enable_wakenet(afe_data);
        }

        ESP_LOGI("AZURE", "---->Converting---");
        detect_flag = 0;
    }
}

printf("detect exit\n");
vTaskDelete(NULL);
}

void app_main()
{

    
    models = esp_srmodel_init("model");
    ESP_ERROR_CHECK(esp_board_init(AUDIO_HAL_08K_SAMPLES, 1, 16));

    // ---- Initialize Wifi ---------
    esp_err_t nvsret = nvs_flash_init();
    if (nvsret == ESP_ERR_NVS_NO_FREE_PAGES || nvsret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        nvsret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(nvsret);

    ESP_LOGI(TAG, "ESP_WIFI_MODE_STA");

    wifi_init_sta();
    // ---- Initialize Wifi Done---------

    // ESP_LOGE("AZURE",  "Azure Token = %s", Token_buffer);

    //----------------File System ------------
    ESP_LOGI(FileTAG, "Initializing SPIFFS");

    esp_vfs_spiffs_conf_t conf = {
        .base_path = "/spiffs",
        .partition_label = NULL,
        .max_files = 5,
        .format_if_mount_failed = true};

    // Use settings defined above to initialize and mount SPIFFS filesystem.
    // Note: esp_vfs_spiffs_register is an all-in-one convenience function.
    esp_err_t ret = esp_vfs_spiffs_register(&conf);

    if (ret != ESP_OK)
    {
        if (ret == ESP_FAIL)
        {
            ESP_LOGE(FileTAG, "Failed to mount or format filesystem");
        }
        else if (ret == ESP_ERR_NOT_FOUND)
        {
            ESP_LOGE(FileTAG, "Failed to find SPIFFS partition");
        }
        else
        {
            ESP_LOGE(FileTAG, "Failed to initialize SPIFFS (%s)", esp_err_to_name(ret));
        }
        return;
    }

    size_t total = 0, used = 0;
    ret = esp_spiffs_info(conf.partition_label, &total, &used);
    if (ret != ESP_OK)
    {
        ESP_LOGE(FileTAG, "Failed to get SPIFFS partition information (%s). Formatting...", esp_err_to_name(ret));
        esp_spiffs_format(conf.partition_label);
        return;
    }
    else
    {
        ESP_LOGI(FileTAG, "Partition size: total: %d, used: %d", total, used);
    }

    // Check consistency of reported partiton size info.
    if (used > total)
    {
        ESP_LOGW(FileTAG, "Number of used bytes cannot be larger than total. Performing SPIFFS_check().");
        ret = esp_spiffs_check(conf.partition_label);
        // Could be also used to mend broken files, to clean unreferenced pages, etc.
        // More info at https://github.com/pellepl/spiffs/wiki/FAQ#powerlosses-contd-when-should-i-run-spiffs_check
        if (ret != ESP_OK)
        {
            ESP_LOGE(FileTAG, "SPIFFS_check() failed (%s)", esp_err_to_name(ret));
            return;
        }
        else
        {
            ESP_LOGI(FileTAG, "SPIFFS_check() successful");
        }
    }

    //--------File System done--------

    afe_handle = &ESP_AFE_SR_HANDLE;
    afe_config_t afe_config = AFE_CONFIG_DEFAULT();

    afe_config.wakenet_model_name = esp_srmodel_filter(models, ESP_WN_PREFIX, NULL);
    afe_config.afe_ringbuf_size = 1024;

#if defined CONFIG_ESP32_S3_BOX_BOARD || defined CONFIG_ESP32_S3_EYE_BOARD
    afe_config.aec_init = false;
#if defined CONFIG_ESP32_S3_EYE_BOARD
    afe_config.pcm_config.total_ch_num = 2;
    afe_config.pcm_config.mic_num = 1;
    afe_config.pcm_config.ref_num = 1;
#endif
#endif

    afe_data = afe_handle->create_from_config(&afe_config);

    task_flag = 1;

    xTaskCreatePinnedToCore(&detect_Task, "detect", 2*8 * 1024, (void *)afe_data, 5, NULL, 1);

    vTaskDelay(3000 / portTICK_PERIOD_MS);

    xTaskCreatePinnedToCore(&feed_Task, "feed", 8 * 1024, (void *)afe_data, 5, NULL, 0);

}
