/* MQTT (over TCP) Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>
#include <stddef.h>
#include "esp_wifi.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "protocol_examples_common.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"

#include "lwip/sockets.h"
#include "lwip/dns.h"
#include "lwip/netdb.h"

#include "esp_log.h"
#include "mqtt_client.h"

#include "driver/gpio.h"
#include "driver/uart.h"

static const char *TAG = "MQTT_EXAMPLE";

/*
# WIFII facultad
- red: CUPPrivada
- pass: complejo7400

# moskitto server
- mqtt://192.168.103.75:1883
*/

#define MQTT_USERNAME "root"               // Replace with your MQTT username
#define MQTT_PASSWORD "remotelabiot"       // Replace with your MQTT password

static const char topic_log[] =  "cenex/lr1/log_device";
static const char topic_data[] = "cenex/lr1/data_device";
static const char topic_post[] = "cenex/lr1/post_value_device";

// TIPO DE MEDICIÓN
#define ON_DEMAND 0
#define MULTI_MEASURE 1

//*************************************************************************************************************************

/*Uart*/
static const int RX_BUF_SIZE = 1024;

#define TXD_PIN (GPIO_NUM_4)
#define RXD_PIN (GPIO_NUM_5)

void init(void) {
    const uart_config_t uart_config = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    // We won't use a buffer for sending data.
    uart_driver_install(UART_NUM_1, RX_BUF_SIZE * 2, 0, 0, NULL, 0);
    uart_param_config(UART_NUM_1, &uart_config);
    uart_set_pin(UART_NUM_1, TXD_PIN, RXD_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
}

int sendData(const char* logName, const char* data)
{
    const int len = strlen(data);
    const int txBytes = uart_write_bytes(UART_NUM_1, data, len);
    // debug
    //ESP_LOGI(logName, "Wrote %d bytes", txBytes);
    return txBytes;
}
/*Uart*/

char device_number[2]="4";

//*************************************************************************************************************************

//queue de mensajes
QueueHandle_t xQueue; 

typedef struct
{
char arrayDatos[30][10];
} Data_t;

//**************************************************************************************************************************

// variable global para poder usarlo entre funciones
esp_mqtt_client_handle_t client = NULL; 
esp_mqtt_client_handle_t mqtt_client_handle = NULL;
esp_mqtt_client_config_t mqtt_client_config = { };
bool mqtt_client_connected = false;
bool mqtt_disconnected_event_flag = false;
char *str;

static void log_error_if_nonzero(const char *message, int error_code)
{
    if (error_code != 0) {
        ESP_LOGE(TAG, "Last error %s: 0x%x", message, error_code);
    }
}

/*
 * @brief Event handler registered to receive MQTT events
 *
 *  This function is called by the MQTT client event loop.
 *
 * @param handler_args user data registered to the event.
 * @param base Event base for the handler(always MQTT Base in this example).
 * @param event_id The id for the received event.
 * @param event_data The data for the event, esp_mqtt_event_handle_t.
 */
static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    ESP_LOGD(TAG, "Event dispatched from event loop base=%s, event_id=%" PRIi32 "", base, event_id);
    BaseType_t xStatus;// agregado paraa queue
    Data_t revice_msg;// datos que se recibien y que se cargan en cola
    
    int msg_id;
    esp_mqtt_event_handle_t event = event_data;
    client = event->client;
    
    switch ((esp_mqtt_event_id_t)event_id) {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
        
        msg_id = esp_mqtt_client_subscribe(client, topic_post, 0);
        ESP_LOGI(TAG, "sent subscribe successful, msg_id=%d", msg_id);

        msg_id = esp_mqtt_client_subscribe(client, topic_data, 1);
        ESP_LOGI(TAG, "sent subscribe successful, msg_id=%d", msg_id);

        break;
    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
        break;

    case MQTT_EVENT_SUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_UNSUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_PUBLISHED:
    	// Debug
        //ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_DATA:
        //ESP_LOGI(TAG, "MQTT_EVENT_DATA");
        if(strncmp(event->topic,topic_post,27)==0){
        	// Debug
        	//printf("TOPIC=%.*s\r\n", event->topic_len, event->topic);
        	//printf("DATA=%.*s\r\n", event->data_len, event->data);
            
        	const char s[5] = "{,\":}"; // separadores de datos
   			char *token; // varable aux para guardar los datos
   			token = strtok(event->data, s);// get the first token 
   			int i=0;// index para sacar todos los campos separados por los caracteres de separación
   			int indexDatos=0;// index para guardar los datos útiles
   			while( token != NULL ) {
   				if((i % 2 != 0)){ // solo guardamos los datos si el index es par y además con i distinto de 0
   		  		strcpy(revice_msg.arrayDatos[indexDatos], token);
   		  		// Debug
   		  		//printf( "Datos desde even data %s\n", token );
   		  		indexDatos++;
   		  		}
   			token = strtok(NULL, s);
   			i++;
  		 	}
  		 	xStatus = xQueueSendToBack( xQueue, &revice_msg, 300);
			if( xStatus != pdPASS )
			{
            //The send operation could not complete because the queue was full -
			//this must be an error as the queue should never contain more than
			//one item! 
			printf( "Could not send to the queue.\n" );
			}
 		}
        break;
    case MQTT_EVENT_ERROR:
        ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
        if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT) {
            log_error_if_nonzero("reported from esp-tls", event->error_handle->esp_tls_last_esp_err);
            log_error_if_nonzero("reported from tls stack", event->error_handle->esp_tls_stack_err);
            log_error_if_nonzero("captured as transport's socket errno",  event->error_handle->esp_transport_sock_errno);
            ESP_LOGI(TAG, "Last errno string (%s)", strerror(event->error_handle->esp_transport_sock_errno));

        }
        break;
    default:
        ESP_LOGI(TAG, "Other event id:%d", event->event_id);
        break;
    }
}

static void mqtt_app_start(void)
{
    esp_mqtt_client_config_t mqtt_cfg = {
       .broker.address.uri = CONFIG_BROKER_URL,
       .credentials.username = MQTT_USERNAME,
       .credentials.authentication.password = MQTT_PASSWORD,
    };
#if CONFIG_BROKER_URL_FROM_STDIN
    char line[128];

    if (strcmp(mqtt_cfg.broker.address.uri, "FROM_STDIN") == 0) {
        int count = 0;
        printf("Please enter url of mqtt broker\n");
        while (count < 128) {
            int c = fgetc(stdin);
            if (c == '\n') {
                line[count] = '\0';
                break;
            } else if (c > 0 && c < 127) {
                line[count] = c;
                ++count;
            }
            vTaskDelay(10 / portTICK_PERIOD_MS);
        }
        mqtt_cfg.broker.address.uri = line;
        printf("Broker url: %s\n", line);
    } else {
        ESP_LOGE(TAG, "Configuration mismatch: wrong broker url");
        abort();
    }
#endif /* CONFIG_BROKER_URL_FROM_STDIN */

    //esp_mqtt_client_handle_t client = esp_mqtt_client_init(&mqtt_cfg);
    client = esp_mqtt_client_init(&mqtt_cfg);
    /* The last argument may be used to pass data to the event handler, in this example mqtt_event_handler */
    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    esp_mqtt_client_start(client);
}

/*Uart*/
// está funcionando todo en esta función
static void rx_task(void *arg)
{
	//mesanje recibido
	/*
	device 4 Laboratorio remoto de óptica
	trama api -> esp32C3
		topic: cenex/lr1/post_value_device
			INICIALIZACIÓN
			{"dev_id":"4","iniciar":"IN"}
			CALIBRACIÓN
			{"dev_id":"4","iniciar":"IN","calibrar":"CA"}
			INICIAR EXPERIMENTO A-B ON DEMAND
			{"dev_id":"4","empezar":"AO"} 
			{"dev_id":"4","empezar":"BO"}
			INICIAR EXPERIMENTO A-B MULTIPLE SAMPLES
			{"dev_id":"4","empezar":"AM"} 
			{"dev_id":"4","empezar":"BM"}
			
	trama esp32C3 -> api
		topic: cenex/lr1/data_device
			{"dev_id":"4","item":[1],intlum":[12]} // ON demand
			{"dev_id":"4","item":[1,2,3,...],intlum":[12,32,1]} Multi
		topic: cenex/lr1/log_device
			{"dev_id":"4","state":"Sistema inicializado"}
	*/

    static const char *RX_TASK_TAG = "RX_TASK";
    static const char *TX_TASK_TAG = "TX_TASK";

    int msg_id; // mqtt

    /*Comandos mqtt*/
    char init[2]= "IN"; // INICIAR
    char cal[2]= "CA"; // INICIAR
    char expAond[2]= "AO"; // EXPA ON DEMAND
    char expBond[2]= "BO"; // EXPB ON DEMAND
    char expAmulti[2]= "AM"; // EXPA MULTI SAMPLES
    char expBmulti[2]= "BM"; // EXPB MULTI SAMPLES
    int8_t size_set_comm = 2; // cantidad de caracteres en comandos mqtt
    /*Comandos mqtt*/
        
    /*separador de trama*/
    char* data = malloc(RX_BUF_SIZE+1);
    char delimitadores[5] = "P;$"; // separadores de datos
    char *token; // varable aux para guardar los datos
   	int8_t index=0;// index para sacar todos los campos separados por los caracteres de separación
   	int8_t indexDatos=0;// index para guardar los datos útiles
    /*separador de trama*/
     
    BaseType_t xStatus;// agregado paraa queue
    Data_t revice_msg;// datos que se recibien y que se cargan en cola
    const TickType_t xTicksToWait = pdMS_TO_TICKS( 1000 ); 
    
	/*Comandos a enviar por uart a LR2*/
	// ESTADO INICIAR
	char commInit_uart_tx[]="P;I;0;0;$"; // trama de inicialización
	char commInit_cal_uart_tx[]="P;I;C;0;$"; // trama de inicialización para calibración
	char commInt_confir[] = "P;I;0;0;$"; // trama de respuesta inicialización
	
	// ESTADO SELECCIÓN DE EXPERIMENTO A o B
	char commE_ExpA_uart_tx[]="P;E;0;0;$"; // trama de enable o empezar experimentoA
	char commE_ExpB_uart_tx[]="P;E;1;0;$"; // trama de enable o empezar experimentoB
	
	char commE_ExpA_confir[] = "P;E;0;0;$"; // trama de respuesta enable o empezar experimentoA
	char commE_ExpB_confir[] = "P;E;1;0;$"; // trama de respuesta enable o empezar experimentoB
	
	// ESTADO MEDICIÓN
	char commM_ON_DEMAND[]="P;M;0;0;$";  // trama de solicitud de medición ON demand
	char commM_MULTI_SAMPLE[]="P;M;1;0;$"; // trama de solicitud de medición Multiple mueasures
	char commM_confir[]="P;M;"; // tomamos solo parte de la trama de solicitud de medición Multiple mueasures
	int size_set_comm_confirm_reduc = 4; // cantidad de caracteres a comparar en trama de recepción commM_confir
	// stop confirmación
	char commStopConfir[] = "P;S;0;0;$"; // trama de confirmación de fin de medición
	
	char comm_uart_tx[10];
	int8_t size_set_comm_confirm = 9; // cantidad de caracteres en trama de recepción
	/*Comandos a enviar por uart a LR2*/
	
	/*Variables aux*/
	int8_t flag_send_data = 0; // Se utiliza para habilitar/ desabilitar en el envío de trama uart TX
	int8_t flag_state = 0; // 0 inicialización | 1 habilitado para empezar 
	int8_t flag_rx_receive = 0; // datos recibidos desde uart
	int8_t flag_log = 0; // 0 experimento sin iniciar | habilitado para informar estado por mqtt
	int8_t flag_measure_type = ON_DEMAND;
	
	// ubicación del campo de interes en la trama "P,D,1,432,$", interera el campo 432
	// no interviene los caracteres P, y ,$ y aranca en cero (relacionado con variable token)
	int8_t measure_value_token = 2; 
	int8_t measure_item_token = 1; 
	/*Variables aux*/
	
	// trama datos experimentales
	char trama_mqtt[1024]="";
	
	//log device mqtt
	char log[128]="";
	char state[128] = "msg no esperado"; // variable global a enviar
	char state_inicializado[] = "Sistema inicializado";
	char state_expAmed_onD[] = "Exp. A en medición...ON_DEMAND";
	char state_expBmed_onD[] = "Exp. B en medición...ON_DEMAND";
	char state_expAmed_multi[] = "Exp. A en medición...MULTI_MEASURE";
	char state_expBmed_multi[] = "Exp. B en medición...MULTI_MEASURE";
	char state_fin[] = "Experimento terminado";

	// datos del experimento de óptica que se envían por mqtt 
	char vector_measure_value[256] = "";
	char vector_measure_item[256] = "";

	while (1)
	{
		// MENSAJE RECIBIDOS DESDE MQTT
		if( uxQueueMessagesWaiting( xQueue ) != 0 )
		{
		// Debug
		// verificamos si se recibieron datos en la cola
		//printf( "Queue should have been empty!\n" );
		}  
		xStatus = xQueueReceive( xQueue, &revice_msg, xTicksToWait );
		if( xStatus == pdPASS )
		{
		// este delay es fundamental para que los datos reciban correctamente
		vTaskDelay(10 / portTICK_PERIOD_MS); // NO BORRAR!!!
		// Debug
		//printf( " Dato1 recibido dsde mqtt %s\n", revice_msg.arrayDatos[0] );
		//printf( " Dato2 recibido dsde mqtt %s\n", revice_msg.arrayDatos[1] );
		int ret = strncmp(revice_msg.arrayDatos[0], device_number, 5);
		//printf("ret: %d \n",ret);
			// TRAMA RECIBIDA DESDE MQTT
			if ( ret==0 ) 
			{
				// Inicialización del sistema
				if ( strncmp(revice_msg.arrayDatos[1], init, 2)==0){
				strcpy(comm_uart_tx,commInit_uart_tx);
					if ( strncmp(revice_msg.arrayDatos[2], cal, 2)==0){
						strcpy(comm_uart_tx,commInit_cal_uart_tx);
					}
				flag_send_data = 1;
				}
				//A
				// Encendido del laser A ON DEMAND
				if ( (strncmp(revice_msg.arrayDatos[1], expAond, size_set_comm) == 0) && (flag_state == 1)){
				strcpy(comm_uart_tx,commE_ExpA_uart_tx);
				flag_measure_type = ON_DEMAND;
				flag_send_data = 1;
				}
				// Encendido del laser A MULTIPLE SAMPLES
				if ( (strncmp(revice_msg.arrayDatos[1], expAmulti, size_set_comm) == 0) && (flag_state == 1)){
				strcpy(comm_uart_tx,commE_ExpA_uart_tx);
				flag_measure_type = MULTI_MEASURE;
				flag_send_data = 1;
				}
				//B
				// Encendido del laser B ON DEMAND
				if ( (strncmp(revice_msg.arrayDatos[1], expBond, size_set_comm)==0) && (flag_state == 1)){
				strcpy(comm_uart_tx, commE_ExpB_uart_tx);
				flag_measure_type = ON_DEMAND;
				flag_send_data = 1;
				}
				// Encendido del laser B MULTIPLE SAMPLES
				if ( (strncmp(revice_msg.arrayDatos[1], expBmulti, size_set_comm)==0) && (flag_state == 1)){
				strcpy(comm_uart_tx, commE_ExpB_uart_tx);
				flag_measure_type = MULTI_MEASURE;
				flag_send_data = 1;
				}
			}
		}
		// MENSAJE RECIBIDOS DESDE UART
		const int rxBytes = uart_read_bytes(UART_NUM_1, data, RX_BUF_SIZE, 10 / portTICK_PERIOD_MS);
		if (rxBytes > 0)
		{
			data[rxBytes] = 0;
			// debug
			ESP_LOGI(RX_TASK_TAG, "Read %d bytes: '%s'", rxBytes, data);
			flag_rx_receive = 1;
		}
		// PROCESAMIENTO DE DATOS RECIBIDOS DESDE UART
		if (flag_rx_receive == 1)
		{
			// CONFIRMACIÓN ESTADO INICIALIZACIÓN
			if (strncmp(data, commInt_confir, size_set_comm_confirm)==0){
				flag_state = 1;
				strcpy(state, state_inicializado);// el sistema se inicializó correctamente
				// debug
				//printf("Estado INICIAR confirmado \n");
			}
			// STOP EXPERIMENTO
			if (strncmp(data, commStopConfir, size_set_comm_confirm)==0){
				strcpy(state, state_fin);// Medición terminada
				flag_state = 0; 
				flag_send_data = 0;
				//debug
				//printf("Experimento terminado, flag_state:%d \n",flag_state);
			}
			//
			// CONFIRMACIÓN ESTADO EMPEZAR EXPA
			if(strncmp(data, commE_ExpA_confir, size_set_comm_confirm)==0){
				if(flag_measure_type == ON_DEMAND){
					strcpy(comm_uart_tx, commM_ON_DEMAND);
					strcpy(state, state_expAmed_onD);// Experimento A en medición
				}
				if(flag_measure_type == MULTI_MEASURE){
					strcpy(comm_uart_tx, commM_MULTI_SAMPLE);
					strcpy(state, state_expAmed_multi);// Experimento A en medición
				}
				flag_send_data = 1;
				//debug
				//printf( " Estado EMPEZAR EXPA confirmado: %s\n", comm_uart_tx);
			}
			
			// CONFIRMACIÓN ESTADO EMPEZAR EXPB
			if(strncmp(data, commE_ExpB_confir, size_set_comm_confirm)==0){
				if(flag_measure_type == ON_DEMAND){
					strcpy(comm_uart_tx, commM_ON_DEMAND);
					strcpy(state, state_expBmed_onD);// Experimento A en medición
				}
				if(flag_measure_type == MULTI_MEASURE){
					strcpy(comm_uart_tx, commM_MULTI_SAMPLE);
					strcpy(state, state_expBmed_multi);// Experimento A en medición
				}
				flag_send_data = 1;
				//debug
				printf( " Estado EMPEZAR EXPB confirmado: %s\n", comm_uart_tx);
			}
			// ESTADO MEDICIÓN
			if (strncmp(data, commM_confir, size_set_comm_confirm_reduc)==0)
			{
				// SE SEPARA EL DATO IntLum DE LA TRAMA RX
				memset(vector_measure_value, '\0', sizeof(vector_measure_value));// vaciamos el string
				memset(vector_measure_item, '\0', sizeof(vector_measure_item));// vaciamos el string
				// se separan los campos provenientes de la trama
				token = strtok(data, delimitadores);// get the first token
				index=0;// index para sacar todos los campos separados por los caracteres de separación
				indexDatos=0;// index para guardar los datos útiles
					while( token != NULL ) {
					//printf( "token %s\n", token );
					// copiamos solo el dato de intensidad luminosa en stringArrayaEnviar
						if(index == measure_value_token){
						// copiamos en el array
						strcat(vector_measure_value,token);
						}
						if(index == measure_item_token){
						// copiamos en el array
						strcat(vector_measure_item,token);
						}
						token = strtok(NULL, delimitadores);
						index++;
					}
				// ARMADO DE LA TRAMA MQTT DATOS
				memset(trama_mqtt, '\0', sizeof(trama_mqtt));// vaciamos el string
				strcat(trama_mqtt, "{ \"dev_id\": ");
				strcat(trama_mqtt, device_number);
				strcat(trama_mqtt, ", \"item\": ");
				strcat(trama_mqtt, vector_measure_item);
				strcat(trama_mqtt, ", \"intlum\": ");
				strcat(trama_mqtt, vector_measure_value);
				strcat(trama_mqtt, " }"); // cierre de la trama de envío mqtt
				// trama mqtt
				//printf("tamaño datos completos a enviar %u\n", strlen(trama_mqtt)); // Vemos el string generado a enviar
				//printf("mqtt data: %s \n",trama_mqtt);
				// envío mqtt
				msg_id = esp_mqtt_client_publish(client, topic_data, trama_mqtt, 0, 1, 0);
				//debug
				//ESP_LOGI(TAG, "sent data publish successful, msg_id=%d", msg_id);
				flag_send_data = 1;
			}
			//
			flag_log = 1;
			flag_rx_receive = 0;
			// debug
			//printf("flag_log: %d | flag_rx_receive: %d \n",flag_log,flag_rx_receive);
		}
		if(flag_send_data == 1)
		{
			printf("comm_uart_tx: %s \n",comm_uart_tx);
			sendData(TX_TASK_TAG, comm_uart_tx);
			flag_send_data = 0;
		}
		// SEND DATA LOG
		if (flag_log == 1)
		{
			//ARMADO DEL MSG LOG
			memset(log, '\0', sizeof(log));// vaciamos el string
			strcat(log, "{ \"dev_id\": ");
			strcat(log, device_number);
			strcat(log, ", \"state\": \"");
			strcat(log, state );
			strcat(log, "\"}");
			//debug
			//printf("log: %s\n",log);
			msg_id = esp_mqtt_client_publish(client, topic_log, log, 0, 1, 0);
			// debug
			//ESP_LOGI(TAG, "sent log publish successful, msg_id=%d", msg_id);
			flag_log = 0;
		}
	}
	free(data);
}
/*Uart*/

void app_main(void)
{
    ESP_LOGI(TAG, "[APP] Startup..");
    ESP_LOGI(TAG, "[APP] Free memory: %" PRIu32 " bytes", esp_get_free_heap_size());
    ESP_LOGI(TAG, "[APP] IDF version: %s", esp_get_idf_version());
    
	// mqtt Queue
    xQueue = xQueueCreate( 1, sizeof( Data_t ) );

    esp_log_level_set("*", ESP_LOG_INFO);
    esp_log_level_set("mqtt_client", ESP_LOG_VERBOSE);
    esp_log_level_set("MQTT_EXAMPLE", ESP_LOG_VERBOSE);
    esp_log_level_set("TRANSPORT_BASE", ESP_LOG_VERBOSE);
    esp_log_level_set("esp-tls", ESP_LOG_VERBOSE);
    esp_log_level_set("TRANSPORT", ESP_LOG_VERBOSE);
    esp_log_level_set("outbox", ESP_LOG_VERBOSE);

    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    ESP_ERROR_CHECK(example_connect());
    
    init();// uart init
    
    mqtt_app_start();  
  
    xTaskCreate(rx_task, "uart_rx_task", 1024*16, NULL, configMAX_PRIORITIES, NULL);
}
