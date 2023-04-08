#ifndef _OPENAI_H_
#define _OPENAI_H_
#include <stdio.h>
#ifdef __cplusplus
extern "C" {
#endif

void _OpenAI_handler(esp_http_client_event_t *evt);
void Generate_dialogue(char *prompt);
void Generate_dialogue_proxy(char *prompt);

#ifdef __cplusplus
}
#endif

#endif