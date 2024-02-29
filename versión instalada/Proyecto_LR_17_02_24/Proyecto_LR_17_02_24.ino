/************************************************************************************************************************
TRAMAS:
  ESTADOS:
    - INICIAR: "P;I;0;0;S" // APAGA LOS LASERS Y SE POSICIONA EN EL CERO CARRO
    - CALIBRAR: "P;I;C;0;S" // SE DESTRABA EL MOTOR Y SE ENCIENDEN LOS LASERS
        - RESPUESTA DEL EQUIPO (INICIAR O CALIBRAR): "P;I;0;0;S" // INICIALIZACION CORRECTA
                                "P;f;0;0;S" // FALLA EN INICIALIZACIÓN
    - EMPEZAR: "P;E;0;0;S" // EXPERIMENTO A ( SE POSICIONA Y ENCIENDE LASER A)
               "P;E;1;0;S" // EXPERIMENTO B ( SE POSICIONA Y ENCIENDE LASER B)
      - RESPUESTA DEL EQUIPO: "P;E;0;0;S" // SE REALIZÓ LA ACCIÓN ExpA
                              "P;E;1;0;S" // SE REALIZÓ LA ACCIÓN ExpB
    - MEDIR:   
      - MEDICIÓN ON DEMAND: "P;M;0;0;S"
      - RESPUESTA DEL EQUIPO: "P;M;1;123;S" // RESPONDE CON EL ITEM DE MUESTRA (CAMPO 3) Y CON EL VALOR MEDIDO (CAMPO 4)
                              "P;S;0;0;S"   // SE COMPLETO EL TOTAL DE MEDICIONES O SE LLEGÓ AL FINAL DE LA CARRERA DEL CARRO
      - MEDICIÓN MULTIPLE MEASURE: "P;M;1;0;S" 
      - RESPUESTA DEL EQUIPO: "P;M;[0,1,2,3,...];[123,32,44,...];S" // RESPONDE CON UN CONJUNTO DE MUESTRAS
                              "P;S;0;0;S"   // SE COMPLETO EL TOTAL DE MEDICIONES O SE LLEGÓ AL FINAL DE LA CARRERA DEL CARRO
**************************************************************************************************************************/
/***********************************
Datasheet
motor NEMA17:          | MS1 MS2 MS3|
              fullstep | (L   L   L | Full Step    | 2 Phase) Pasos totales en carrera 2006
                       | (L   H   L | Quarter Step | W1-2 Phase) Pasos totales en carrera 8024
***********************************/

/**********************************
Dimensiones de equipo:
						L=851+16=867mm
						recorrido 403mm
						Laser 650 nm
            largo de un experimento 141 mm
            para la configuración actual (una medición cada 10 step) se optienen 2790 step
            por lo tanto el factor de conversión para el eje x del grafico de intensidad es 141/2790
            El factor de conversion para el eje y es 10 (escala la intensidad de 0 a 100 %)
**********************************/

/*************************************
Arduino NANO 
compilación: OLD bootloader
*************************************/
// 
#define FULL_STEP 0 // full Step
#define QUARTER_STEP 1 // quarter Step
#define MAX_STEP_FULL_STEP 2006  // en este modo la carrera total del sensor es de 2006 step
#define MAX_STEP_QUARTER_STEP 8024 // en este modo la carrera total del sensor es de 8024 step
// porcentaje de recorrido de medición respecto al maximo total
// por ejemplo para la confoiguración FULL_STEP para cada experimento se dispone de la mitad de la carrera total
// de esta manera 0,4 *2006 da un recorrido de 802 step para el experimento A
#define PORCENTAJE_CARRERA 0.35 // recorrido durante el ensayo ( en este caso la mitad del recorrido)
#define PORCENTAJE_CARRERA_INIT_A 0.05 // se separa 20 mm del I_INICIAL
#define PORCENTAJE_CARRERA_INIT_B 0.5 // se ubica a la mitad del recorrido total

#define ITEM_COUNT_MAX 2    // cantidad de mediciones por envio de trama en cada medicion
#define PASOS_MEDICION 10   // cuanto avanza el carro entre medición y medición
#define ENABLE_EXPA 0 // estado empezar
#define ENABLE_EXPB 1 // estado empezar

//Declarando pines de entrada/salida
//Pines de control de motores
//Motor de Pickup (Cabezal de adquisición de intensidad lumínica)
#define EN1   10 // ENABLE 
#define STEP1 11
#define DIR1  12 // DIRECCIÓN
#define GIRO_HORARIO 0  // HACIA FINAL DE CARRERA I_FINAL
#define GIRO_ANTIHORARIO 1 // HACIA FINAL DE CARRERA I_INICIAL

//Motor de selección de blanco (Intercambio de diferentes elementos difractantes 
//(Regillas, alambres, etc.))
#define EN2   7
#define STEP2 8
#define DIR2  9
//Pines de Salida digitales
//Selección de Laser
#define LASER_A 5 
#define LASER_B 6

//Pines Entrada/salida digitales
#define I_INICIO  2    
#define I_FINAL  3            
#define STEP  4    
//Entradas analógicas
#define A_AUX1   A1

#define TIME_STEP 1 // ms
#define INIT_TIMEOUT 50000
//-------------------------------------------------------------------------------------------------------------------

// CARGAR VARIABLE step segun configuración del motor (ver linea 32-33)
int step = 1;
int max_step_item;

//Variables globales de hardware
int x_pos_meassure = 0;
int pasos_init_A;
int pasos_init_B;
int pasos_p_inicializacion=20;//defecto



// trama de lectura
String tramaRead;
int flag_trama_avail = 0;

// estado iniciar
bool flag_iniciar=false;
bool flag_med=false;
char iniciar[] = "I"; // Iniciar
char calibracion[] = "C";
String Trama_write_init_conf_ok = "P;I;0;0;$";
String Trama_write_init_conf_fail= "P;F;0;0;$";

// estado empezar
int get_laser = 0; // variable interna del estado empezar
char empezar[] = "E";  // EMpezar
char laser_A[] ="0"; // Experimento A en trama: P,E,0,0,$ el PRIMER 0 corresponde a ExpA
char laser_B[] ="1"; // Experimento B
String Trama_write_empExpA_conf = "P;E;0;0;$";
String Trama_write_empExpB_conf = "P;E;1;0;$";

// estado medir
char medir[] = "M"; // medir
char on_demand[]= "0";
char multi_samples[] = "1";
//VECTOR SAMPLE
char vector_measure_value[128];
char vector_measure_item[128];
char actual_value[10];
char actual_item[10];
int item =0; // cantidad de mediciones
String Trama_write_endExp_conf = "P;S;0;0;$"; // Stop

/*separador de trama*/
const char delimitadores[5] = "P;$"; // separadores de datos
char *token; // varable aux para guardar los datos
int index=0;// index para sacar todos los campos separados por los caracteres de separación
int indexDatos=0;// index para guardar los datos útiles
// datos del experimento de óptica que se envían por mqtt 
char ArrayDatosRX[30][10];
/*separador de trama*/

void setup() {
  hardSetup();
  Serial.begin(115200);
}

void loop() {
    // TRAMA DISPONIBLE EN PUERTO SERIE
    if (Serial.available()>0){
        tramaRead = Serial.readString();//Lee trama a partir del puerto serie
        // debug
        //Serial.println("trama RX:"+tramaRead);
        // se separan los campos provenientes de la trama
		    token = strtok(tramaRead.c_str(), delimitadores);// get the first token
		    index=0;// index para sacar todos los campos separados por los caracteres de separación
		    indexDatos=0;// index para guardar los datos útiles
		    while( token != NULL ) {
			    strcpy(ArrayDatosRX[index],token);
			    token = strtok(NULL, delimitadores);
			    index++;
		    }
        flag_trama_avail = 1; // si hay trama en buffer rx se procesa la información
    }
    // ESTADOS DE FUNCIONAMIENTO
    if (flag_trama_avail == 1)
    {
      // ESTADO INICIAR
      if(strncmp(ArrayDatosRX[0], iniciar,1)==0){
      // mover a posición cero
      flag_iniciar = set_zero_carro_pos();
        // enviar confirmación
        if(flag_iniciar == true){
          // Enviar trama  de Inicialización exitosa
          Serial.println(Trama_write_init_conf_ok);
        }
        if(flag_iniciar == false){
          // Enviar trama de Falla en inicialización
          Serial.println(Trama_write_init_conf_fail);
        }
        // SUB ESTADO CALIBRACIÓN
        // motor desbloqueado
        // laser ensendidos
          if(strncmp(ArrayDatosRX[1], calibracion,1)==0){
            laser_EXPA_ON();
            laser_EXPB_ON();
          }
      }
      // ESTADO EMPEZAR EXPERIMENTO
      if(strncmp(ArrayDatosRX[0], empezar, 1)==0){
        // encender laser
        get_laser = set_laser();
          if(get_laser == ENABLE_EXPA){
          // envia trama de confirmación
          Serial.println(Trama_write_empExpA_conf);
          }
          if(get_laser == ENABLE_EXPB){
          // envia trama de confirmación
          Serial.println(Trama_write_empExpB_conf);
          }
      }
      // ESTADO MEDICIÓN
      if(strncmp(ArrayDatosRX[0], medir, 1)==0){
        // UNA MEDICIÓN POR PEDIDO DE DATO
        if(strncmp(ArrayDatosRX[1], on_demand, 1)==0){
          // mover carro
          // obtener medición de Intensidad luminosa
          // envia trama de medición
          // envíar trama de finalización
          flag_med = carro_mov_on_demand();
            if(flag_med  == true){
              laser_off();
              carro_disable_down(); // enable OFF
              // trama de terminación experimento
              Serial.println(Trama_write_endExp_conf);
            }
        }
        // MULTIPLES MEDICIONES 
        if(strncmp(ArrayDatosRX[1], multi_samples, 1)==0){
          // mover carro
          // obtener medición de Intensidad luminosa
          // envia trama de medición
          // envíar trama de finalización
          flag_med = carro_mov_multi_samples();
            if(flag_med  == true){
              laser_off();
              carro_disable_down(); // enable OFF
              // trama de terminación experimento
              Serial.println(Trama_write_endExp_conf);
            }
        }
      }
      flag_trama_avail = 0;
    }  
}
/***********************************************************************************************/
void hardSetup()
{
    if(step == FULL_STEP){
    max_step_item = PORCENTAJE_CARRERA * MAX_STEP_FULL_STEP;
    pasos_init_A = PORCENTAJE_CARRERA_INIT_A * MAX_STEP_FULL_STEP;
    pasos_init_B = PORCENTAJE_CARRERA_INIT_B * MAX_STEP_FULL_STEP;
    }
    if(step == QUARTER_STEP){
    max_step_item = PORCENTAJE_CARRERA * MAX_STEP_QUARTER_STEP;
    pasos_init_A = PORCENTAJE_CARRERA_INIT_A * MAX_STEP_QUARTER_STEP;
    pasos_init_B = PORCENTAJE_CARRERA_INIT_B * MAX_STEP_QUARTER_STEP;
    }
    //max_step_item = max_step_item / PASOS_MEDICION; // esto es porque en cada medición se avanza mas de un step
  //Incialización de entrada/salida;
    //Pines de control de motores
        //Motor de Pickup (Cabezal de adquisición de intensidad lumínica)
            pinMode(EN1,OUTPUT); digitalWrite(EN1,1); //Inicalizo el motor 1 DESHABILITADO
            pinMode(STEP1,OUTPUT);
            pinMode(DIR1,OUTPUT);
        //Motor de selección de blanco (Intercambio de diferentes elementos difractantes (Regillas, alambres, etc.))
            pinMode(EN2,OUTPUT); digitalWrite(EN1,1); //Inicalizo el motor 2 DESHABILITADO
            pinMode(STEP2,OUTPUT);
            pinMode(DIR2,OUTPUT);
        //Pines de Salida digitales
            pinMode(I_INICIO,INPUT);    
            pinMode(I_FINAL,INPUT);            
        //Selección de Laser
            pinMode(LASER_A,OUTPUT); digitalWrite(LASER_A,0); 
            pinMode(LASER_B,OUTPUT); digitalWrite(LASER_B,0);
        //Pines Entrada/salida digitales
            pinMode(STEP,OUTPUT); digitalWrite(STEP,step);   
}
//___________________________________________________________________________________________________________________

// FUNCIONES
bool set_zero_carro_pos(void)
{
  // se lleva el sensor a la posición 0 y luego se posiciona el sensor según el experimento
   long TimeOut = millis ();
   bool falla = false;
   laser_off();
   carro_enable_set(GIRO_ANTIHORARIO);
   while (!(digitalRead(I_INICIO)))
   {
      carro_mov_step();
      if (millis() > TimeOut + INIT_TIMEOUT) {falla = true; break;} // verifica que el fin de carrera fue presionado o que paso un timeout
   }
    carro_disable_down(); // enable OFF
    //item =(-1)*PASOS_MEDICION; // esto es para que el primer item sea igual a 0
    item = 0;
    return(true);
}

int set_laser(void)
{
  int experimento = 0; // 0-> expA | 1 -> expB 
  // selección de pasos iniciales según experimento A o B
  if (strncmp(ArrayDatosRX[1], laser_A, 1) == 0){
    pasos_p_inicializacion=pasos_init_A;// experimento A 
    laser_EXPA_ON();
    experimento = 0;
  }
  if (strncmp(ArrayDatosRX[1], laser_B, 1) == 0){
    pasos_p_inicializacion=pasos_init_B;// experimento B 
    laser_EXPB_ON();
    experimento = 1;
  }
  // mov carro según Experimento seleccionado
  carro_enable_set(GIRO_HORARIO);
  for (int j = 1; j < pasos_p_inicializacion; j++)
    {
      carro_mov_step(); 
    }
    return experimento;
}
bool carro_mov_on_demand (void)
{
  // x_pos no puede inicializarse aquí
  // porque sino cada vez que se pide la función 
  // se reiniciaría
  //Movimiento de motor y adquisición
  if (!(digitalRead(I_FINAL)))
  {
      for(int i=0;i<PASOS_MEDICION;i++){
        carro_mov_step();
      }
      // este if debe estar antes del envío de trama 
      // para que no se envía trama si se desbordó
      if (item > max_step_item){
        item = 0;
        return true; // fin experimento por desborde de muestras
      }
      item = item+PASOS_MEDICION; 
      //Envío de trama
      Serial.print("P;M;[");
      Serial.print(item);
      Serial.print("];[");
      Serial.print((1024-analogRead(A_AUX1)));
      Serial.println("];$");
      return false; // medición correcta
  }else
  {
    return true; // fin experimento por final de carrera
  }
}

bool carro_mov_multi_samples (void)
{
  // preparamos el char a enviar
	memset(vector_measure_value, '\0', sizeof(vector_measure_value));// vaciamos buffer vector_sample
	strcat( vector_measure_value, "[" ); // Agregamos al principio un caracter
  // vector measure_item
  memset(vector_measure_item, '\0', sizeof(vector_measure_item));// vaciamos buffer vector_sample
  strcat( vector_measure_item, "[" ); // Agregamos al principio un caracter
  //Movimiento de motor y adquisición
  for(int i = 0; i < ITEM_COUNT_MAX; i++)
  {
    if(!(digitalRead(I_FINAL)))
    {
      for(int i=0;i<PASOS_MEDICION;i++){
        carro_mov_step();
      }
      // vector measure
      itoa((1024-analogRead(A_AUX1)), actual_value, 10); // convertir int to char
      strcat(vector_measure_value,actual_value); // agregar a
      strcat(vector_measure_value,",");
      // vector measure_item
      itoa(item, actual_item, 10); // convertir int to char
      strcat(vector_measure_item,actual_item); // agregar a
      strcat(vector_measure_item,",");
    }else
      {
        item = 0;
        return true; // piso el fin de carrera
      }
    item = item + PASOS_MEDICION;
    if (item >= max_step_item){
      item = 0;
      return true; // piso el fin de carrera
    }
  }
    // Se elimina la última coma agregada
		if(strlen(vector_measure_value) > 0) {
			vector_measure_value[strlen(vector_measure_value) - 1] = '\0';
      vector_measure_item[strlen(vector_measure_item) - 1] = '\0';
		}
    strcat( vector_measure_value, "]" ); // Agregamos al final un caracter
    strcat( vector_measure_item, "]" ); // Agregamos al final un caracter

    Serial.print("P;M;");
    Serial.print(vector_measure_item);
    Serial.print(";");
    Serial.print(vector_measure_value);
    Serial.println(";$");
    return false; // medición correcta
}
// funciones aux
void carro_mov_step(void)
{
  digitalWrite(STEP1,HIGH);
  delay(TIME_STEP);
  digitalWrite(STEP1,LOW);
  delay(TIME_STEP);   
}
void carro_enable_set(int giro)
{
   digitalWrite(EN1,1); delay(TIME_STEP); // enable OFF
   digitalWrite(DIR1,giro); delay(TIME_STEP); //Giro hacia posición zero I_inicio
   digitalWrite(EN1,0); delay(TIME_STEP); // enable ON
}
void carro_disable_down(void)
{
  digitalWrite(EN1,1); delay(TIME_STEP); // enable OFF
}
void laser_off(void)
{
  digitalWrite(LASER_A,0);// apagado de laser
  digitalWrite(LASER_B,0);// apagado de laser
}
void laser_EXPA_ON(void)
{
    digitalWrite(LASER_A,1);// apagado de laser
}
void laser_EXPB_ON(void)
{
    digitalWrite(LASER_B,1);// apagado de laser
}
