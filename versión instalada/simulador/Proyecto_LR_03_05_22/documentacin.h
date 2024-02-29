/* INTERFAZ DE COMUNICACIÓN DE LABORATORIOS REMOTOS
 *  ACCIÓN: INCORPORACIÓN DEL CONTROL DEL LASER - RECUPERACIÓN DEL ARCHIVO PRINCIPAL
 *  FECHA: 24/12/21
 *  AUTOR: COGLIATTI, JUAN IGNACIO
 *  -----------------------------------------------------------------------
 * U L T I M A S   M O D I F I C A C I O N E S 
 * 03/05/22: se agregó la nomenclaruta Experimento A o B A=verde(led_interno_placa)=1 | B=rojo(led_interno_placa)=0
 *           se gregó el set inicial del sensor según si es experimento A o B
 *           
 *           PARA HACER EL EXPERIMENTO A se debe enviar:
 *           P,A,1,0,$
 *           luego 
 *           P,R,1,0,$
 *   
 *           PARA HACER EL EXPERIMENTO B se debe enviar:
 *           P,A,0,0,$
 *           luego 
 *           P,R,0,0,$
 *           
 *  3/3 Modif. en la función trama_verif(): anulación de verificación
 *  17/3: modificación en la función extraer_campos():
 *     -> Uso de punteros para portar datos
 *     -> se agregó un nuevo parámetro para almacenar el laser deseado
 *     Moificación en el programa principal
 *     -> se agregó la variable laser_sel para almacenal el laser deseado
 *     Modif. de la función ensayo(int estadoint *ptr_laser_sel)
 *     -> incorporación del puntero para la selección de laser     
 *     NOTA: LASER VERDE conectado a entrada analógica n°2 a_aux2
 *     
 *  ----------------------------------------------------------------------- 
 *  VECTOR DE ESTADOS:
 *  NOMBRE: estado
 *  TIPO: int
 *  DATOS DEL VECTOR:
 *  ----->   1: 0 0 0 0 0 0 0 1: Datos disponibles por puerto serie
 *  ----->   2: 0 0 0 0 0 0 1 0: Trama correcta
 *  ----->   4: 0 0 0 0 0 1 0 0: Trama ARM identificada 
 *  ----->   8: 0 0 0 0 1 0 0 0: Trama RUN identificada
 *  ----->  16: 0 0 0 1 0 0 0 0: Armado correcto
 *  ----->  32: 0 0 1 0 0 0 0 0: Trama ACK identificada
 *  ----->  64: 0 1 0 0 0 0 0 0: Ensayo en marcha
 *  -----> 128: 1 0 0 0 0 0 0 0: NULL
 *  -----------------------------------------------------------------------
 *  TRAMA DE DATOS ENTRANTE:
 *  --> P, msg , valorLaser ,valorRanuraAlambre , $
 *      |   |        |               |            |
 *      |   |        |               |             ----> COLA DE TRAMA 
 *      |   |        |                ----> Selección de Ranura/Alambre                                                 
 *      |   |         ----> Selección del color del LASER (0 laser rojo Experimento B | 1 laser verde Experimento A)
 *      |    -----> R: Iniciar ensayo
 *      |      |                
 *      |       --> A: Inicializar el hardware
 *      |      |
 *      |       --> O: ACK, enviar el siguiente dato
 *       ---->ENCABEZADO                                                 
 *  
 *  -----------------------------------------------------------------------
 *  TRAMA DE DATOS SALIENTE:
 *  --> P, msg , x_pos ,intensidad , $
 *      |   |        |      |        |
 *      |   |        |      |         ----> COLA DE TRAMA 
 *      |   |        |      |                                                         
 *      |   |        |       ----> intensidad lumínica 
 *      |   |         ----> posición en x                                                 
 *      |    -----> O: inicialización del hardware correcta
 *      |      |                
 *      |       --> F: fallo en inicialización del hardware
 *      |      |
 *      |       --> D: mensaje enviado con cada punto muestra
 *      |      |
 *      |       --> S: STOP, fin de ensayo
 *       ---->ENCABEZADO  
 *       
 *   -----------------------------------------------------------------------    
 *   Conectando un cable entre GND y el Pin digital 9 se simula un error en el armado
 *   el sistema retornará una trama de error de la forma: P,F,#,#,$
 *   
 *     []<-------->[]
 *      G           D9  
 *      N      
 *      D
 *   -----------------------------------------------------------------------    
 */
