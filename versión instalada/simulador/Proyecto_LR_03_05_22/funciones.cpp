#include <Arduino.h>
#include <LiquidCrystal.h>
#include "hardware.h"

//DISPLAY LCD para depuración
  const int rs = 12, en = 11, d4 = 5, d5 = 4, d6 = 3, d7 = 2;
  LiquidCrystal lcd1(rs, en, d4, d5, d6, d7);

// d e f i n i c i ó n   d e   v a r i a b l e s 

//Definiendo punteros para usar la función strtok()
    char *strtok(char *str1, const char *str2);
    char *resultado = NULL;
    char delimitadores[] = ",";

//Definiendo Variables para almacenar los campos
    char inicio = NULL;
    char campo1 = NULL;
    char campo2 = NULL;
    char campo3 = NULL;
    char    fin = NULL;

//Definiendo variables según su función
    char valorLaser = NULL;    //Define color de laser
    char valorRanuraAlambre = NULL; // Define ensayo de regilla o alambre
    char msg = NULL; //Flag de Inicio de Ensayo

//Generando variable índice para recorrer la trama
    int i = 0;

//I M P L E M E N T A C I O N   D E    F U N C I O N E S

//MOSTRAR POR LCD
void lcdMuestra (int estado){
   lcd1.clear();
   lcd1.setCursor(0, 0);
   lcd1.print("estado = ");
   //lcd1.setCursor(0, 1);
   lcd1.print(estado);
   //delay(500);
   }

//_____________________________________________________________________________________________________________________
//Inicializa las variables para la recepción de una nueva trama
void Limpiar_campos (){
    //Preparo los campos para el ingreso de datos nuevos
                    inicio = NULL;
                    campo1 = NULL;
                    campo2 = NULL;
                    campo3 = NULL;
                       fin = NULL;
                         i = 0;

    //Limpio variables
                       msg = NULL;
        valorRanuraAlambre = NULL;
                valorLaser = NULL;
}
//_____________________________________________________________________________________________________________________

int trama_verif (String trama, int estado)
{
          estado = estado - 1;
              lcdMuestra(estado);
// modif 3/3/22 eliminación de control de trama              
//          if ((trama.charAt ( trama.length()-1) == '$' ) && (trama.charAt(0) == 'P' )) {
              estado = estado + 2;
              lcdMuestra(estado);
              Limpiar_campos (); 
//           }
           return(estado);
}
//_____________________________________________________________________________________________________________________


void disp_setup(){

   // Configuración de hardware
     hardSetup();

   //Inicialización del canal de comunicación con el servidor
     Serial.begin(115200);

   //Inicializando el panel de cristal líquido (borrar despues)
     lcd1.begin(16, 2);
     lcd1.clear();
     lcd1.print("Sistema Enc!");

}
//_____________________________________________________________________________________________________________________
//Divide la trama en sus campos y los almacena en las variables.
int Extraer_campos (String trama, int estado,int *ptr_laser_sel){
  //Extrayendo el primer campo
    resultado = strtok(trama.c_str() , delimitadores);  //trama.c_str(): Convierte el string a char 
  //Bucle de extracción del resto de los campos
    while ( resultado != NULL ) {
      switch (i){
        case 0:
          inicio = *resultado;   // aparentemente *resultado lee el valor almacenado en la posición de memoria
          break;                 // y resultado lee la posición de ,memoria donde apunta el puntero     
        case 1:
          campo1 = *resultado;
          break;
        case 2:
          campo2 = *resultado;
          break;
        case 3:
          campo3 = *resultado;
          break;
        case 4:
          fin = *resultado;
          break;
    }
    i++;
    resultado = strtok ( NULL , delimitadores );
    }
   //Selección de laser
   switch (campo2) {
    case '1':{
      *ptr_laser_sel = 1;
      break;}
     case '0':{
      *ptr_laser_sel = 0;
       break;}
   } 
    switch (campo1)
    {
      case 'R':
      {
        if (estado == 18) 
        {
             ensayo(8,ptr_laser_sel); //condición de START
             return(8);
        }
        else
            return(0);
      break;
      }
      case 'O':
      {
        ensayo(32,ptr_laser_sel); //condición de CONTINUACION
        return(32);
      break;
      }
      case 'A':
      {
          bool arm_ok = armado(ptr_laser_sel);
          if (arm_ok) 
          {
                Serial.print('P');Serial.print(',');Serial.print('A');
                Serial.print(',');Serial.print(campo2);Serial.print(',');
                Serial.print(campo3);Serial.print(',');Serial.println('$');
            return(16);   
          }
          else 
          {
                Serial.print('P');Serial.print(',');Serial.print('F');
                Serial.print(',');Serial.print(campo2);Serial.print(',');
                Serial.print(campo3);Serial.print(',');Serial.println('$');
            return(0);
          }
      break;
      }
   }
 }  

//---------------------------------------------------------------------------------------------------------------------- 
