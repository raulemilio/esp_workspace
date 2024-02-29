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

#include "esp_adc/adc_continuous.h"

#include "driver/gpio.h"

static const char *TAG = "MQTT_EXAMPLE";

#define MQTT_USERNAME "root"               // Replace with your MQTT username
#define MQTT_PASSWORD "remotelabiot"               // Replace with your MQTT password

//*************************************************************************************************************************
#define GPIO_OUTPUT_IO_0    18
#define GPIO_OUTPUT_IO_1    19
#define GPIO_OUTPUT_IO_2    10
#define GPIO_OUTPUT_PIN_SEL  ((1ULL<<GPIO_OUTPUT_IO_0) | (1ULL<<GPIO_OUTPUT_IO_1)| (1ULL<<GPIO_OUTPUT_IO_2))
#define GPIO_INPUT_IO_0     4  // final de carrera fondo
#define GPIO_INPUT_IO_1     5  // final de carrera frente
#define GPIO_INPUT_PIN_SEL  ((1ULL<<GPIO_INPUT_IO_0) | (1ULL<<GPIO_INPUT_IO_1))
#define ESP_INTR_FLAG_DEFAULT 0

static QueueHandle_t gpio_evt_queue = NULL;

static void IRAM_ATTR gpio_isr_handler(void* arg)
{
    uint32_t gpio_num = (uint32_t) arg;
    xQueueSendFromISR(gpio_evt_queue, &gpio_num, NULL);
}
// guarda de seguridad para límites de carrera del sensor
static void gpio_task_example(void* arg)
{
    uint32_t io_num;
    int cnt=0;
    for(;;) {
        if(xQueueReceive(gpio_evt_queue, &io_num, portMAX_DELAY)) {
            //printf("GPIO[%"PRIu32"] intr, val: %d\n", io_num, gpio_get_level(io_num));
            gpio_set_level(GPIO_OUTPUT_IO_2,0); // enable motor
            if(io_num==4){
            gpio_set_level(GPIO_OUTPUT_IO_1,1); // mover al frente
				for(int32_t i=0;i<30;i++){
					printf("cnt: %d\n", cnt++);
					gpio_set_level(GPIO_OUTPUT_IO_0, cnt % 2);
					vTaskDelay(1/portTICK_PERIOD_MS);
				}
				cnt = 0;
            }
            if(io_num==5){
            gpio_set_level(GPIO_OUTPUT_IO_1,0); // mover al fondo
				for(int32_t i=0;i<30;i++){
					printf("cnt: %d\n", cnt++);
					gpio_set_level(GPIO_OUTPUT_IO_0, cnt % 2);
					vTaskDelay(1/portTICK_PERIOD_MS);
				}
				cnt = 0;
            }
        gpio_set_level(GPIO_OUTPUT_IO_2,1); // disable motor
        }
    }
}
//*************************************************************************************************************************

char device_number[12]="3";

//*************************************************************************************************************************
#define EXAMPLE_ADC_UNIT                    ADC_UNIT_1
#define _EXAMPLE_ADC_UNIT_STR(unit)         #unit
#define EXAMPLE_ADC_UNIT_STR(unit)          _EXAMPLE_ADC_UNIT_STR(unit)
#define EXAMPLE_ADC_CONV_MODE               ADC_CONV_SINGLE_UNIT_1
#define EXAMPLE_ADC_ATTEN                   ADC_ATTEN_DB_0
#define EXAMPLE_ADC_BIT_WIDTH               SOC_ADC_DIGI_MAX_BITWIDTH

#if CONFIG_IDF_TARGET_ESP32 || CONFIG_IDF_TARGET_ESP32S2
#define EXAMPLE_ADC_OUTPUT_TYPE             ADC_DIGI_OUTPUT_FORMAT_TYPE1
#define EXAMPLE_ADC_GET_CHANNEL(p_data)     ((p_data)->type1.channel)
#define EXAMPLE_ADC_GET_DATA(p_data)        ((p_data)->type1.data)
#else
#define EXAMPLE_ADC_OUTPUT_TYPE             ADC_DIGI_OUTPUT_FORMAT_TYPE2
#define EXAMPLE_ADC_GET_CHANNEL(p_data)     ((p_data)->type2.channel)
#define EXAMPLE_ADC_GET_DATA(p_data)        ((p_data)->type2.data)
#endif

#define EXAMPLE_READ_LEN                    128

#if CONFIG_IDF_TARGET_ESP32
static adc_channel_t channel[1] = {ADC_CHANNEL_6};
#else
static adc_channel_t channel[1] = {ADC_CHANNEL_2};
#endif

static TaskHandle_t s_task_handle;
//static const char *TAG = "EXAMPLE";

//queue de mensajes
QueueHandle_t xQueue; 

typedef struct
{
char arrayDatos[30][10];
} Data_t;

static bool IRAM_ATTR s_conv_done_cb(adc_continuous_handle_t handle, const adc_continuous_evt_data_t *edata, void *user_data)
{
    BaseType_t mustYield = pdFALSE;
    //Notify that ADC continuous driver has done enough number of conversions
    vTaskNotifyGiveFromISR(s_task_handle, &mustYield);

    return (mustYield == pdTRUE);
}

static void continuous_adc_init(adc_channel_t *channel, uint8_t channel_num, adc_continuous_handle_t *out_handle)
{
    adc_continuous_handle_t handle = NULL;

    adc_continuous_handle_cfg_t adc_config = {
        .max_store_buf_size = 4096,
        .conv_frame_size = EXAMPLE_READ_LEN,
    };
    ESP_ERROR_CHECK(adc_continuous_new_handle(&adc_config, &handle));

    adc_continuous_config_t dig_cfg = {
        .sample_freq_hz = 620, // ver en sdkconfig valores máximos y mínimos (611Hz-83333Hz)
        .conv_mode = EXAMPLE_ADC_CONV_MODE,
        .format = EXAMPLE_ADC_OUTPUT_TYPE,
    };

    adc_digi_pattern_config_t adc_pattern[SOC_ADC_PATT_LEN_MAX] = {0};
    dig_cfg.pattern_num = channel_num;
    for (int i = 0; i < channel_num; i++) {
        adc_pattern[i].atten = EXAMPLE_ADC_ATTEN;
        adc_pattern[i].channel = channel[i] & 0x7;
        adc_pattern[i].unit = EXAMPLE_ADC_UNIT;
        adc_pattern[i].bit_width = EXAMPLE_ADC_BIT_WIDTH;

        ESP_LOGI(TAG, "adc_pattern[%d].atten is :%"PRIx8, i, adc_pattern[i].atten);
        ESP_LOGI(TAG, "adc_pattern[%d].channel is :%"PRIx8, i, adc_pattern[i].channel);
        ESP_LOGI(TAG, "adc_pattern[%d].unit is :%"PRIx8, i, adc_pattern[i].unit);
    }
    dig_cfg.adc_pattern = adc_pattern;
    ESP_ERROR_CHECK(adc_continuous_config(handle, &dig_cfg));

    *out_handle = handle;
}
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
        
        msg_id = esp_mqtt_client_subscribe(client, "cenex/lr1/post_value_device", 0);
        ESP_LOGI(TAG, "sent subscribe successful, msg_id=%d", msg_id);

        msg_id = esp_mqtt_client_subscribe(client, "cenex/lr1/data_device", 1);
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
        if(strncmp(event->topic,"cenex/lr1/post_value_device",27)==0){
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
//tarea read adc
static void read_adc_task(void)
{
	//mesanje recibido
	//{"dev_id":"1","tiempo_registro":"LR"}
	// dev: device number
	// tiempoRegistro: LR->long register (10s)| SR-> sort registe (3s)
	ESP_LOGI(TAG, "Ingresa a read_adc_task()");
	int msg_id;
	int count=0;
	int i=0;// contador para enviar estado del dispositivo
	BaseType_t xStatus;// agregado paraa queue
	Data_t revice_msg;// datos que se recibien y que se cargan en cola
	const TickType_t xTicksToWait = pdMS_TO_TICKS( 1000 );
	//**************************************************************************************
	esp_err_t ret;
	uint32_t ret_num = 0;
	uint8_t result[EXAMPLE_READ_LEN] = {0};
	memset(result, 0xcc, EXAMPLE_READ_LEN);
	s_task_handle = xTaskGetCurrentTaskHandle();

	adc_continuous_handle_t handle = NULL;
	continuous_adc_init(channel, sizeof(channel) / sizeof(adc_channel_t), &handle);

	adc_continuous_evt_cbs_t cbs = {
	.on_conv_done = s_conv_done_cb,
	};
	ESP_ERROR_CHECK(adc_continuous_register_event_callbacks(handle, &cbs, NULL));
	ESP_ERROR_CHECK(adc_continuous_start(handle));
	//**************************************************************************************
	while(1){
	ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
	char unit[] = EXAMPLE_ADC_UNIT_STR(EXAMPLE_ADC_UNIT);

	if( uxQueueMessagesWaiting( xQueue ) != 0 )
	{
	// verificamos si se recibieron datos en la cola
	// Debug
	//printf( "Queue should have been empty!\n" );
	}
	xStatus = xQueueReceive( xQueue, &revice_msg, xTicksToWait );

	if( xStatus == pdPASS )
	{
	// Debug
	// Data was successfully received from the queue, print out the received value. 
	//printf( " Dato1 recibido dsde la cola %s\n", revice_msg.arrayDatos[0] );
	//printf( " Dato2 recibido dsde la cola %s\n", revice_msg.arrayDatos[1] );
	//printf( " Dato3 recibido dsde la cola %s\n", revice_msg.arrayDatos[2] );
	//printf( " Dato3 recibido dsde la cola %s\n", revice_msg.arrayDatos[3] );
	
	int ret = strncmp(revice_msg.arrayDatos[0], device_number, 5);
	//printf("ret %d\n", ret);
	   if ( ret==0 ){
		//Tiempo de registro
		if ( strncmp(revice_msg.arrayDatos[1], "LR", 2)==0){
		count=1000;
		}
		if ( strncmp(revice_msg.arrayDatos[1], "SR", 2)==0){
		count=300;
		}
		char datosAenviar[512];
		// Convertimos a String el array de datos
		char stringArrayaEnviar[1024] = {0};

		for(int i=0;i<count;i++) {
		ret = adc_continuous_read(handle, result, EXAMPLE_READ_LEN, &ret_num, 0);
			if (ret == ESP_OK) {
			datosAenviar[0] = 0;
			// Agregamos al principio un caracter
			strcpy( stringArrayaEnviar, "[" ); 
			for (int i = 0; i < ret_num; i += SOC_ADC_DIGI_RESULT_BYTES) {
			adc_digi_output_data_t *p = (adc_digi_output_data_t*)&result[i];
			uint32_t data = EXAMPLE_ADC_GET_DATA(p);
			sprintf( &stringArrayaEnviar[ strlen(stringArrayaEnviar) ],  "%li, ", data);
			}
			// El último dato debe agregarse a mano
			sprintf( &stringArrayaEnviar[ strlen(stringArrayaEnviar) ],  "%u", result[(sizeof(result)/sizeof(uint8_t)-1)] );
			// Agregamos al final el caracter de terminación
			strcat( stringArrayaEnviar, "]" ); 
			//printf("tamaño array en string a enviar %u\n", strlen(stringArrayaEnviar)); // Vemos el string generado a enviar
			strcat(datosAenviar, "{ \"dev_id\": ");
			strcat(datosAenviar, "1");
			strcat(datosAenviar, ", \"voltaje\": ");
			strcat(datosAenviar, stringArrayaEnviar);
			strcat(datosAenviar, " }");
			// Debug
			//printf("tamaño datos completos a enviar %u\n", strlen(datosAenviar)); // Vemos el string generado a enviar
			msg_id = esp_mqtt_client_publish(client, "cenex/lr1/data_device", datosAenviar, 0, 1, 0);
			//ESP_LOGI(TAG, "sent publish successful, msg_id=%d", msg_id);
			}
		vTaskDelay(10);
		}
	  }
	}
	else
	{
	// Data was not received
	// Debug 
	//printf( "Could not receive from the queue.\n" );
		if(i==1000){
			msg_id = esp_mqtt_client_publish(client, "cenex/lr1/log_device", "{\"dev_id\":\"1\",\"state\":\"1\"}", 0, 1, 0);
			ESP_LOGI(TAG, "sent publish successful, msg_id=%d", msg_id);
		i=0;
		}
		i++;
	}
	}
	ESP_ERROR_CHECK(adc_continuous_stop(handle));
	ESP_ERROR_CHECK(adc_continuous_deinit(handle));
	vTaskDelete(NULL);
}

static void servo_ind_sensor_task(void)
{
	int msg_id;
	BaseType_t xStatus;// agregado paraa queue
	Data_t revice_msg;// datos que se recibien y que se cargan en cola
	//mesanje recibido
	//{"dev_id":"3","sentido_giro":"F","paso_motor":"LP"}
	// dev: device number
	// sentidoGiro: F->front chasis | B-> back chasis
	// pasos_motor: LP-> long passs | SP-> slow pass
	const TickType_t xTicksToWait = pdMS_TO_TICKS( 1000 ); 
	int32_t pasos_motor=0; // 0: 1X | 1: 2X
	
	//zero-initialize the config structure.
	gpio_config_t io_conf = {};
	//disable interrupt
	io_conf.intr_type = GPIO_INTR_DISABLE;
	//set as output mode
	io_conf.mode = GPIO_MODE_OUTPUT;
	//bit mask of the pins that you want to set,e.g.GPIO18/19
	io_conf.pin_bit_mask = GPIO_OUTPUT_PIN_SEL;
	//disable pull-down mode
	io_conf.pull_down_en = 0;
	//disable pull-up mode
	io_conf.pull_up_en = 0;
	//configure GPIO with the given settings
	gpio_config(&io_conf);

    //interrupt of rising edge
    io_conf.intr_type = GPIO_INTR_POSEDGE;
    //bit mask of the pins, use GPIO4/5 here
    io_conf.pin_bit_mask = GPIO_INPUT_PIN_SEL;
    //set as input mode
    io_conf.mode = GPIO_MODE_INPUT;
    //enable pull-up mode
    io_conf.pull_up_en = 1;
    gpio_config(&io_conf);

    //change gpio interrupt type for one pin

    gpio_set_intr_type(GPIO_INPUT_IO_0, GPIO_INTR_ANYEDGE);
 
    //create a queue to handle gpio event from isr
    gpio_evt_queue = xQueueCreate(10, sizeof(uint32_t));
    //start gpio task
    //xTaskCreate(gpio_task_example, "gpio_task_example", 2048, NULL, 10, NULL);

    //install gpio isr service
    gpio_install_isr_service(ESP_INTR_FLAG_DEFAULT);
    //hook isr handler for specific gpio pin
    gpio_isr_handler_add(GPIO_INPUT_IO_0, gpio_isr_handler, (void*) GPIO_INPUT_IO_0);
    //hook isr handler for specific gpio pin
    gpio_isr_handler_add(GPIO_INPUT_IO_1, gpio_isr_handler, (void*) GPIO_INPUT_IO_1);

    //remove isr handler for gpio number.
    //gpio_isr_handler_remove(GPIO_INPUT_IO_0);
    //hook isr handler for specific gpio pin again
    //gpio_isr_handler_add(GPIO_INPUT_IO_0, gpio_isr_handler, (void*) GPIO_INPUT_IO_0);
    
	int i=0;// contador para enviar estado del dispositivo
	int cnt = 0;
	gpio_set_level(GPIO_OUTPUT_IO_2,1); // disable motor
	while(1) {
	if( uxQueueMessagesWaiting( xQueue ) != 0 )
	{
	// verificamos si se recibieron datos en la cola
	printf( "Queue should have been empty!\n" );
	}  
	xStatus = xQueueReceive( xQueue, &revice_msg, xTicksToWait );
	 if( xStatus == pdPASS )
	{
	// Debug
	// Data was successfully received from the queue, print out the received value. 
	//printf( " Dato1 recibido dsde la cola %s\n", revice_msg.arrayDatos[0] );
	//printf( " Dato2 recibido dsde la cola %s\n", revice_msg.arrayDatos[1] );
	//printf( " Dato3 recibido dsde la cola %s\n", revice_msg.arrayDatos[2] );
	//printf( " Dato3 recibido dsde la cola %s\n", revice_msg.arrayDatos[3] );
	
	int ret = strncmp(revice_msg.arrayDatos[0], device_number, 5);
	printf("ret %d\n", ret);
		if ( ret==0 ){
			gpio_set_level(GPIO_OUTPUT_IO_2,0); // enable motor
			//sentido de giro
			if ( strncmp(revice_msg.arrayDatos[1], "B", 1)==0){
			gpio_set_level(GPIO_OUTPUT_IO_1,0);
			}
			if ( strncmp(revice_msg.arrayDatos[1], "F", 1)==0){
			gpio_set_level(GPIO_OUTPUT_IO_1,1);
			}
			if ( strncmp(revice_msg.arrayDatos[2], "LP", 2)==0){
			pasos_motor=2600;
			}
			if ( strncmp(revice_msg.arrayDatos[2], "SP", 2)==0){
			pasos_motor=200;
			}
				for(int32_t i=0;i<pasos_motor;i++){
					// Debug
					printf("cnt: %d\n", cnt++);
					gpio_set_level(GPIO_OUTPUT_IO_0, cnt % 2);
					vTaskDelay(1/portTICK_PERIOD_MS);
				}
				cnt = 0;
		gpio_set_level(GPIO_OUTPUT_IO_2,1); // disable motor
		}
	}
	else
	{
	// Debug
	// Data was not received 
	//printf( "Could not receive from the queue.\n" );
		if(i==1000){
			msg_id = esp_mqtt_client_publish(client, "cenex/lr1/log_device", "{\"dev_id\":\"3\",\"state\":\"1\"}", 0, 1, 0);
			ESP_LOGI(TAG, "sent publish successful, msg_id=%d", msg_id);
		i=0;
		}
		i++;
	}
  }
}

static void servo_wheel_task(void)
{
   BaseType_t xStatus;// agregado paraa queue
   Data_t revice_msg;// datos que se recibien y que se cargan en cola
	int msg_id;
   //mesanje recibido
   //{"dev_id":"2","sentido_giro":"I","velocidad_motor":"HV"}
   // dev: device number
   // sentidoGiro: I->izquierdo (visto de frente) | D-> derecho (visto de frente)
   // velocity_motor: HV-> higth velocity | SV-> slow velocity 
   const TickType_t xTicksToWait = pdMS_TO_TICKS( 1000 ); 

    //zero-initialize the config structure.
    gpio_config_t io_conf = {};
    //disable interrupt
    io_conf.intr_type = GPIO_INTR_DISABLE;
    //set as output mode
    io_conf.mode = GPIO_MODE_OUTPUT;
    //bit mask of the pins that you want to set,e.g.GPIO18/19
    io_conf.pin_bit_mask = GPIO_OUTPUT_PIN_SEL;
    //disable pull-down mode
    io_conf.pull_down_en = 0;
    //disable pull-up mode
    io_conf.pull_up_en = 0;
    //configure GPIO with the given settings
    gpio_config(&io_conf);

    //printf("Minimum free heap size: %"PRIu32" bytes\n", esp_get_minimum_free_heap_size());
	int i=0;// contador para enviar estado del dispositivo
    int cnt = 0;
    gpio_set_level(GPIO_OUTPUT_IO_2,1); // disable motor
    int velocidad_motor=0;
    while(1) {
        if( uxQueueMessagesWaiting( xQueue ) != 0 )
	{
	// Debug
	// verificamos si se recibieron datos en la cola
	//printf( "Queue should have been empty!\n" );
	}  
	xStatus = xQueueReceive( xQueue, &revice_msg, xTicksToWait );
	if( xStatus == pdPASS )
	{
	// Debug
	// Data was successfully received from the queue, print out the received value. 
	//printf( " Dato1 recibido dsde la cola %s\n", revice_msg.arrayDatos[0] );
	//printf( " Dato2 recibido dsde la cola %s\n", revice_msg.arrayDatos[1] );
	//printf( " Dato3 recibido dsde la cola %s\n", revice_msg.arrayDatos[2] );
	//printf( " Dato3 recibido dsde la cola %s\n", revice_msg.arrayDatos[3] );
	
	int ret = strncmp(revice_msg.arrayDatos[0], device_number, 5);
	printf("ret %d\n", ret);
		if ( ret==0 ){
			gpio_set_level(GPIO_OUTPUT_IO_2,0); // enable motor
			//sentido de giro
			if ( strncmp(revice_msg.arrayDatos[1], "I", 1)==0){
			gpio_set_level(GPIO_OUTPUT_IO_1,0);
			}
			if ( strncmp(revice_msg.arrayDatos[1], "D", 1)==0){
			gpio_set_level(GPIO_OUTPUT_IO_1,1);
			}
			if ( strncmp(revice_msg.arrayDatos[2], "SV", 2)==0){
			velocidad_motor=20;
			}
			if ( strncmp(revice_msg.arrayDatos[2], "HV", 2)==0){
			velocidad_motor=12;
			}
			// Debug
			printf("Velocidad motor: %d\n\r",velocidad_motor);
				for(int i=0;i<6350;i++){// dos vueltas
					// Debug
					printf("cnt: %d\n", cnt++);
        			gpio_set_level(GPIO_OUTPUT_IO_0, cnt % 2);
        			vTaskDelay(velocidad_motor/portTICK_PERIOD_MS);
        		}
        		cnt = 0;
        	gpio_set_level(GPIO_OUTPUT_IO_2,1); // disable motor
        	}
	}
	else
	{
	// Data was not received
	// Debug 
	//printf( "Could not receive from the queue.\n" );
		if(i==1000){
			msg_id = esp_mqtt_client_publish(client, "cenex/lr1/log_device", "{\"dev_id\":\"2\",\"state\":\"1\"}", 0, 1, 0);
			ESP_LOGI(TAG, "sent publish successful, msg_id=%d", msg_id);
		i=0;
		}
		i++;
	}

    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "[APP] Startup..");
    ESP_LOGI(TAG, "[APP] Free memory: %" PRIu32 " bytes", esp_get_free_heap_size());
    ESP_LOGI(TAG, "[APP] IDF version: %s", esp_get_idf_version());

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
//
    mqtt_app_start();
//    read_adc_task(); // sensor inductivo device 1
    servo_ind_sensor_task(); // servo sensor inductivo device 3
//    servo_wheel_task(); // servo ruedas device 2
//  status_device();
}
