#include <stdio.h>
#include <stdlib.h>
#include <sys/param.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_event.h"

#include "esp_http_client.h"
#include "esp_tls.h"
//#include "base64.h"

#include "cJSON.h"
#include "openai.h"
#include "Secert.h"

#define TAG "OPENAI"
#define MAX_HTTP_OUTPUT_BUFFER 2048

extern const char openai_cert_pem_start[] asm("_binary_openai_pem_start");

void _OpenAI_handler(esp_http_client_event_t *evt)
{
    static char *output_buffer; // Buffer to store response of http request from event handler
    static int output_len;      // Stores number of bytes read
    switch (evt->event_id)
    {
    case HTTP_EVENT_ERROR:
        ESP_LOGI(TAG, "HTTP_EVENT_ERROR");
        break;
    case HTTP_EVENT_ON_CONNECTED:
        break;
    case HTTP_EVENT_HEADER_SENT:
        break;
    case HTTP_EVENT_ON_HEADER:
       // ESP_LOGI(TAG, "HTTP_EVENT_ON_HEADER, key=%s, value=%s", evt->header_key, evt->header_value);
        break;
    case HTTP_EVENT_ON_DATA:
        /*
         *  Check for chunked encoding is added as the URL for chunked encoding used in this example returns binary data.
         *  However, event handler can also be used in case chunked encoding is used.
         */
        if (!esp_http_client_is_chunked_response(evt->client))
        {
            // If user_data buffer is configured, copy the response into the buffer
            int copy_len = 0;
            if (evt->user_data)
            {
                copy_len = MIN(evt->data_len, (MAX_HTTP_OUTPUT_BUFFER - output_len));
                if (copy_len)
                {
                    memcpy(evt->user_data + output_len, evt->data, copy_len);
                }
            }
            else
            {
                const int buffer_len = esp_http_client_get_content_length(evt->client);
                if (output_buffer == NULL)
                {
                    //ESP_LOGI(TAG, "Data got: %d", buffer_len);
                    output_buffer = (char *)malloc(buffer_len);
                    output_len = 0;
                    if (output_buffer == NULL)
                    {
                        ESP_LOGE(TAG, "Failed to allocate memory for output buffer");
                        return ESP_FAIL;
                    }
                }
                copy_len = MIN(evt->data_len, (buffer_len - output_len));
                if (copy_len)
                {
                    memcpy(output_buffer + output_len, evt->data, copy_len);
                }
            }
            output_len += copy_len;
        }

        break;
    case HTTP_EVENT_ON_FINISH:
        // ESP_LOGI(TAG, "HTTP_EVENT_ON_FINISH");
        if (output_buffer != NULL)
        {
            cJSON *json = cJSON_Parse(output_buffer);

            char *id = cJSON_GetObjectItem(json, "id")->valuestring;
            char *object = cJSON_GetObjectItem(json, "object")->valuestring;

            cJSON *choices = cJSON_GetObjectItem(json, "choices");
            int choicesArraySize = cJSON_GetArraySize(choices);
            for (int i = 0; i < choicesArraySize; i++)
            {
                cJSON *choice = cJSON_GetArrayItem(choices, i);
                char *text = cJSON_GetObjectItem(choice, "text")->valuestring;
                ESP_LOGW(TAG, "----TEXT--> # [ %s ] ", text+2*sizeof(char));
                // int index = cJSON_GetObjectItem(choice, "index")->valueint;
                char *finish_reason = cJSON_GetObjectItem(choice, "finish_reason")->valuestring;
            }

            // ESP_LOGE("HTTP CB:", "handle buffer");
            // Response is accumulated in output_buffer. Uncomment the below line to print the accumulated response
            // ESP_LOG_BUFFER_HEX(TAG, output_buffer, output_len);
            cJSON_Delete(json);
            free(output_buffer);
            output_buffer = NULL;
        }
        // ESP_LOGE("HTTP EXIT CB:", "Goodbye CB");
        output_len = 0;
        break;
    case HTTP_EVENT_DISCONNECTED:
        /*
        int mbedtls_err = 0;
        esp_err_t err = esp_tls_get_and_clear_last_error((esp_tls_error_handle_t)evt->data, &mbedtls_err, NULL);
        if (err != 0)
        {
            ESP_LOGI(TAG, "Last esp error code: 0x%x", err);
            ESP_LOGI(TAG, "Last mbedtls failure: 0x%x", mbedtls_err);
        }*/
        if (output_buffer != NULL)
        {
            free(output_buffer);
            output_buffer = NULL;
        }
        output_len = 0;
        break;
    }
    // return ESP_OK;
}

void Generate_dialogue(char *prompt)
{

   // ESP_LOGW(TAG, "Connecting AI ");

    cJSON *root = cJSON_CreateObject();

    cJSON_AddStringToObject(root, "prompt", prompt);
    cJSON_AddStringToObject(root, "model", "text-davinci-003");
    cJSON_AddNumberToObject(root, "temperature", 0.1);
    cJSON_AddNumberToObject(root, "max_tokens", 200);
    cJSON_AddNumberToObject(root, "top_p", 1);
    cJSON_AddNumberToObject(root, "frequency_penalty", 0.3);
    cJSON_AddNumberToObject(root, "presence_penalty", 0.0);

    char *json_str = cJSON_Print(root);

    esp_http_client_config_t OpenAI = {
        .url = OPENAI_ENDPOINT,
        .transport_type = HTTP_TRANSPORT_OVER_SSL,
        .event_handler = _OpenAI_handler,
        .cert_pem = openai_cert_pem_start,
    };

    esp_http_client_config_t config = OpenAI;
    esp_http_client_handle_t client = esp_http_client_init(&config);
    esp_http_client_set_method(client, HTTP_METHOD_POST);

    esp_http_client_set_header(client, "Authorization", OPENAI_TOKEN);

    esp_http_client_set_header(client, "Content-Type", "application/json");

    // esp_http_client_set_header(client, "Content-Length",Para->payload_len);

    esp_http_client_set_post_field(client, json_str, sizeof(json_str));

    esp_err_t err = esp_http_client_perform(client);

    if (err == ESP_OK)
    {
        // ESP_LOGE("HTTP CALL:", "-0 Ending HTTP!");
        // ESP_LOGI(TAG, "HTTPS Status = %u, content_length = %u",
        //         esp_http_client_get_status_code(client),
        //         esp_http_client_get_content_length(client));
    }
    else
    {
        ESP_LOGE(TAG, "Error perform http request %s", esp_err_to_name(err));
    }

    esp_http_client_cleanup(client);

    cJSON_Delete(root);
    free(json_str);
}

void Generate_dialogue_proxy(char *prompt)
{

    //ESP_LOGW(TAG, "Connecting AI PROXY");

    cJSON *root = cJSON_CreateObject();

    cJSON_AddStringToObject(root, "prompt", prompt);
    cJSON_AddStringToObject(root, "model", "text-davinci-003");
    cJSON_AddNumberToObject(root, "temperature", 0.9);
    cJSON_AddNumberToObject(root, "max_tokens", 80);
    cJSON_AddNumberToObject(root, "top_p", 1);
    cJSON_AddNumberToObject(root, "frequency_penalty", 0.3);
    cJSON_AddNumberToObject(root, "presence_penalty", 0.0);

    char *json_str = cJSON_Print(root);
    // ESP_LOGW(TAG, "JSON: %s", json_str);

    esp_http_client_config_t OpenAI_Proxy = {
        .url = OPENAI_PROXY_ENDPOINT,
        .event_handler = _OpenAI_handler,
        .timeout_ms = 10000,
    };

    esp_http_client_config_t config = OpenAI_Proxy;
    esp_http_client_handle_t client = esp_http_client_init(&config);
    esp_http_client_set_method(client, HTTP_METHOD_POST);

    // esp_http_client_set_header(client, "Authorization", OPENAI_TOKEN);

    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_header(client, "Accept-Encoding", "gzip, deflate, br");
    // size_t json_len = strlen(json_str);
    // ESP_LOGE(TAG, "Length %d", json_len);
    char payload_len_str[30];
    sprintf(payload_len_str, "%d", strlen(json_str));

    //  ESP_LOGE(TAG, "Length %s", payload_len_str);

    /*
        size_t b64_len = base64_encode_len(strlen(json_str));
        char *b64_str = malloc(b64_len + 1);
        if (b64_str == NULL) {
            ESP_LOGE(TAG, "Failed to allocate memory");
            free(json_str);
            esp_http_client_cleanup(client);
            return;
        }
        base64_encode((const unsigned char *)json_str, strlen(json_str), b64_str, &b64_len, 0);
    */
    esp_http_client_set_header(client, "Content-Length", payload_len_str);

    esp_http_client_set_post_field(client, json_str, strlen(json_str));

    esp_err_t err = esp_http_client_perform(client);

    if (err == ESP_OK)
    {
        // ESP_LOGE("HTTP CALL:", "-0 Ending HTTP!");
        // ESP_LOGI(TAG, "HTTPS Status = %u, content_length = %u",
        //         esp_http_client_get_status_code(client),
        //         esp_http_client_get_content_length(client));
    }
    else
    {
        ESP_LOGE(TAG, "Error perform http request %s", esp_err_to_name(err));
    }

    esp_http_client_cleanup(client);

    cJSON_Delete(root);
    free(json_str);
}