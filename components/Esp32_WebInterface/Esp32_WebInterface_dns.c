/*
  Esp32_WebInterface_dns.c
*/

#include <string.h>
#include <sys/socket.h>
#include <netdb.h>
#include "esp_log.h"
#include "Esp32_WebInterface.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "DNS_SERVER";
static TaskHandle_t dns_task_handle = NULL; // Riferimento al task

// --- SERVER DNS SEMPLICE
void dns_server_task(void *pvParameters)
{
    uint8_t data[128];
    struct sockaddr_in servaddr, clientaddr;
    socklen_t clilen = sizeof(clientaddr);

    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0)
    {
        ESP_LOGE(TAG, "Impossibile creare il socket");
        vTaskDelete(NULL);
        return;
    }

    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(53);

    if (bind(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0)
    {
        ESP_LOGE(TAG, "Bind fallito sulla porta 53");
        close(sockfd);
        vTaskDelete(NULL);
        return;
    }

    ESP_LOGI(TAG, "DNS Server avviato su porta 53...");

    while (1)
    {
        int n = recvfrom(sockfd, data, sizeof(data), 0, (struct sockaddr *)&clientaddr, &clilen);
        if (n > 12)
        {
            data[2] |= 0x80; // QR = 1 (Risposta)
            data[3] |= 0x80; // RA = 1
            data[7] = 1;     // Answer count = 1

            uint8_t answer[] = {0xc0, 0x0c, 0x00, 0x01, 0x00, 0x01, 0x00, 0x00, 0x00, 0x3c, 0x00, 0x04, 192, 168, 4, 1};
            memcpy(data + n, answer, sizeof(answer));
            sendto(sockfd, data, n + sizeof(answer), 0, (struct sockaddr *)&clientaddr, clilen);
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }

    // Pulizia (teorica, il task viene solitamente cancellato dall'esterno)
    close(sockfd);
    vTaskDelete(NULL);
}

// Funzioni per avviare e fermare il server DNS
void WebInterface_StartDNS(WebInterface_t *self)
{
    if (dns_task_handle == NULL)
    {
        xTaskCreate(dns_server_task, "dns_server", 4096, NULL, 5, &dns_task_handle);
    }
}

// Funzione per fermare il server DNS, se necessario
void WebInterface_StopDNS(WebInterface_t *self)
{
    if (dns_task_handle != NULL)
    {
        ESP_LOGI(TAG, "Arresto del server DNS...");
        vTaskDelete(dns_task_handle);
        dns_task_handle = NULL;
    }
}