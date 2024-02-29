  //Incluyendo Liquidcristal para debug (Borrar después)
#include <LiquidCrystal.h>

//Incluyendo funciones
#include "funciones.h"


//Vector de estados
int estado = 0;
int estado_aux = 0;

//variable de selección de laser
int laser_sel = 5;

//trama de datos:
String trama;

void setup() {
  //Inicializando chip Atmega328P (Microcontrolador)  
     disp_setup();
}

void loop() {
  // put your main code here, to run repeatedly:
  //Recibe un string por puerto serie  
    if (Serial.available()){
        trama = Serial.readString(); //Lee trama a partir del puerto serie
        estado =  estado + 1;
              lcdMuestra(estado);
    }
  //Mostrando trama y desidiendo si es correcta
  int res = estado % 2;
    if ( res == 1 )
    { 
        estado = trama_verif(trama,estado);
        if (estado == 2) estado = Extraer_campos(trama,estado,&laser_sel);
        if (estado == 10) estado = Extraer_campos(trama,estado,&laser_sel);
        if (estado == 18) estado = Extraer_campos(trama,estado,&laser_sel);
        if (estado == 34) estado = Extraer_campos(trama,estado,&laser_sel);
        lcdMuestra(estado);
     }
 
}
