/* BSD Socket API Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include "log_server.h"

#define PORT CONFIG_LOG_SERVER_PORT

static const char *TAG = "LOG_SERVER";

QueueHandle_t log_q = NULL;

int sock_log(const char* string, va_list list){

    char log_str[1024];
    memset(log_str,0,sizeof(log_str));
    int len = vsnprintf(log_str,sizeof(log_str),string,list);

    for(int i = 0; i < len; i++){
        xQueueSend(log_q,&log_str[i],10);
    }
    
    return len;

}

static void log_recv(int *sock)
{
    int len;
    char rx_buffer[128];

    do {
        len = recv(*sock, rx_buffer, sizeof(rx_buffer) - 1, 0);
        if (len < 0) {
            ESP_LOGE(TAG, "Error occurred during receiving: errno %d", errno);
        } else if (len == 0) {
            ESP_LOGW(TAG, "Connection closed");
        } else {
            rx_buffer[len] = 0; // Null-terminate whatever is received and treat it like a string
            ESP_LOGI(TAG, "Received %d bytes: %s", len, rx_buffer);
        }
    } while (len > 0);

    shutdown(*sock, 0);
    close(*sock);
    *sock = -1;
}

void server_recv(void *arg){
    int *socket = (int*)arg;
    esp_log_set_vprintf(sock_log);
    log_recv(socket);
    esp_log_set_vprintf(vprintf);
    vTaskDelete(NULL);
}

void server_send(void *arg){
    int *socket = (int*)arg;
    uint32_t i = 0;

    while(*socket > 0){

        int waiting = uxQueueMessagesWaiting(log_q);
        if(waiting == 0){
            vTaskDelay(1);
            continue;
        }

        char rx_buffer[1024];
        memset(rx_buffer,0,sizeof(rx_buffer));
        for(i = 0; i < waiting; i++){
            xQueueReceive(log_q,&rx_buffer[i],portMAX_DELAY);
        }

        int to_write = waiting;
        while (to_write > 0) {
            int written = send(*socket, rx_buffer + (waiting - to_write), to_write, 0);
            if (written < 0) {
                ESP_LOGE(__func__, "Socket %d Error occurred during sending: errno %d",*socket, errno);
                shutdown(*socket, 0);
                close(*socket);
                *socket = -1;
                break;
            }
            to_write -= written;
        }

    }
    vTaskDelete(NULL);

}

static void tcp_server_task(void *pvParameters)
{
    char addr_str[128];
    int addr_family = (int)pvParameters;
    int ip_protocol = 0;
    
    struct sockaddr_in dest_addr_ip4;
    dest_addr_ip4.sin_addr.s_addr = htonl(INADDR_ANY);
    dest_addr_ip4.sin_family = AF_INET;
    dest_addr_ip4.sin_port = htons(PORT);
    ip_protocol = IPPROTO_IP;


    int listen_sock = socket(addr_family, SOCK_STREAM, ip_protocol);
    if (listen_sock < 0) {
        ESP_LOGE(TAG, "Unable to create socket: errno %d", errno);
        vTaskDelete(NULL);
        return;
    }

    ESP_LOGI(TAG, "Socket created");

    int err = bind(listen_sock, (struct sockaddr *)&dest_addr_ip4, sizeof(dest_addr_ip4));
    if (err != 0) {
        ESP_LOGE(TAG, "Socket unable to bind: errno %d", errno);
        ESP_LOGE(TAG, "IPPROTO: %d", addr_family);
        goto CLEAN_UP;
    }
    ESP_LOGI(TAG, "Socket bound, port %d", PORT);

    err = listen(listen_sock, 1);
    if (err != 0) {
        ESP_LOGE(TAG, "Error occurred during listen: errno %d", errno);
        goto CLEAN_UP;
    }

    while (1) {

        ESP_LOGI(TAG, "Socket listening");

        struct sockaddr_storage source_addr;
        uint addr_len = sizeof(source_addr);
        int sock = accept(listen_sock, (struct sockaddr *)&source_addr, &addr_len);
        if (sock < 0) {
            ESP_LOGE(TAG, "Unable to accept connection: errno %d", errno);
            break;
        }

        // Convert ip address to string
        inet_ntoa_r(((struct sockaddr_in *)&source_addr)->sin_addr, addr_str, sizeof(addr_str) - 1);

        ESP_LOGI(TAG, "Socket accepted ip address: %s", addr_str);

        char task_name[64];
        memset(task_name,0,sizeof(task_name));
        snprintf(task_name,64,"Socket %d Task",sock);

        xTaskCreate(server_recv, "task_name", 4096, &sock, 4, NULL);
        xTaskCreate(server_send, "task_name", 4096, &sock, 4, NULL);
    }

CLEAN_UP:
    close(listen_sock);
    vTaskDelete(NULL);
}

void start_log_server(void)
{
    log_q = xQueueCreate(2048,1);

    xTaskCreate(tcp_server_task, "tcp_server", 4096, (void*)AF_INET, 5, NULL);

}
