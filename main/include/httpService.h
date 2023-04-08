#ifndef HTTPSERVICE_H
#define HTTPSERVICE_H
#include <stdio.h>
#ifdef __cplusplus
extern "C" {
#endif

extern char *Token_buffer;



typedef struct {
    esp_http_client_config_t httpCfg;
    char* payload; 
    char* payload_len;
    int Method;
    char* Auth; 
    char* ContentType;
    char* Response;
    int Resp_len;

} HttpTaskParas;


void AzureTokenFetch(esp_http_client_config_t config,  char* payload, int payload_len);
void _http_event_handler(esp_http_client_event_t *evt);
void HttpUpload(void *Paras);
void HttpAzureSpeech2Text(void *Paras);
void _Azure_Speech_event_handler(esp_http_client_event_t *evt);
esp_err_t _CommonHttpCpyResp(esp_http_client_event_t *evt);
esp_err_t _Azure_Token_cb_handler(esp_http_client_event_t *evt);
void HttpGetAzureToken(HttpTaskParas* cfg, char* KEY);


#ifdef __cplusplus
}
#endif

#endif