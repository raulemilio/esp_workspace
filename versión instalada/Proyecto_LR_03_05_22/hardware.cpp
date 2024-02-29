#include <Arduino.h>
//Variables globales de hardware
             int x_pos = 0;
             int pasos_p_inicializacion=20;//defecto
//Defino pasos por cada punto adquirido
            #define pasos_p_adquisicion 3
            #define pasos_p_inicializacion_experimentoA_laserVerde 90
            #define pasos_p_inicializacion_experimentoB_laserRojo  1003// los pasos totales son 2007             

//Declarando pines de entrada/salida
    //Pines de control de motores
        //Motor de Pickup (Cabezal de adquisición de intensidad lumínica)
            #define EN1   10
            #define STEP1 11
            #define DIR1  12
        //Motor de selección de blanco (Intercambio de diferentes elementos difractantes 
        //(Regillas, alambres, etc.))
            #define EN2   7
            #define STEP2 8
            #define DIR2  9
    //Pines de Salida digitales
          //Selección de Laser
            #define Laser_Verde A2 //ANTES 5
            #define Laser_Rojo  6
    //Pines Entrada/salida digitales
            #define I_inicio  3    
            #define I_final  2            
            #define D_aux  4    
    //Entradas analógicas
            #define Int_Luz  A0
            #define A_AUX1   A1
            #define A_AUX2   A2
            #define A_AUX3   A3
//-------------------------------------------------------------------------------------------------------------------


void hardSetup()
{
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
            pinMode(I_inicio,INPUT);    
            pinMode(I_final,INPUT);            
        //Selección de Laser
            pinMode(Laser_Verde,OUTPUT); digitalWrite(Laser_Verde,0);
            pinMode(Laser_Rojo,OUTPUT); digitalWrite(Laser_Rojo,0); 
        //Pines Entrada/salida digitales
            pinMode(D_aux,OUTPUT); digitalWrite(D_aux,0);   
}
//___________________________________________________________________________________________________________________

void ensayo(int estado,int *ptr_laser_sel)
{
   char sel = 'N';
   //if (x_pos < 5) {

    if (( estado == 8 ) and (x_pos == 0))  sel = 'R';      //estado de START
    else if (( estado == 32 ) && (x_pos >= 0) && (!(digitalRead(I_final)))) sel = 'O';   //estado de CONTINUACIÓN
    //else sel = 'N';}
    else  sel = 'S';  //estado de STOP
    
    switch(sel){
      case 'N':
      break;
      case 'R':{
          //Enciendo Laser_Rojo/verde CUANDO INICIA EL ENSAYO
             if (*ptr_laser_sel == 1)digitalWrite(Laser_Rojo,1);// experimento B 
             else if (*ptr_laser_sel == 0)digitalWrite(Laser_Verde,1);// experimento A 
          //Envío de trama
          Serial.print('P');Serial.print(',');Serial.print("D");
          Serial.print(',');Serial.print(x_pos++);Serial.print(',');
          Serial.print((1024-analogRead(A_AUX1)));Serial.print(',');
          Serial.println('$');          
      break;}
      case 'O':{
         //Movimiento de motor y adquisición
           digitalWrite(EN1,1); delay(5);
           digitalWrite(DIR1,0); delay(5);
           digitalWrite(EN1,0); delay(5); //Cambio de giro
           for (int j = 1; j < pasos_p_adquisicion; j++)
           {
             digitalWrite(STEP1,HIGH);
             delay(5);
             digitalWrite(STEP1,LOW);
             delay(5);    
           }
            digitalWrite(EN1,1); delay(5); //deshabilito
          //Envío de trama
          Serial.print('P');Serial.print(',');Serial.print("D");
          Serial.print(',');Serial.print(x_pos++);Serial.print(',');
          Serial.print((1024-analogRead(A_AUX1)));Serial.print(',');
          Serial.println('$');
      break;}
      case 'S':{
          //Movimiento de motor y adquisición
           digitalWrite(EN1,1); delay(5); 
           digitalWrite(DIR1,1); delay(5); 
           digitalWrite(EN1,0); delay(5); //Cambio de giro
           for (int j = 1; j < pasos_p_inicializacion; j++)
           {
             digitalWrite(STEP1,HIGH);
             delay(5);
             digitalWrite(STEP1,LOW);
             delay(5);    
           }
           digitalWrite(EN1,1);
          //Envío de trama
          Serial.print('P');Serial.print(',');Serial.print("S");
          Serial.print(',');Serial.print(x_pos++);Serial.print(',');
          Serial.print((1024-analogRead(A_AUX1)));Serial.print(',');
          Serial.println('$');
          //apago Laser_Rojo/verde CUANDO INICIA EL ENSAYO
             if (*ptr_laser_sel == 1) digitalWrite(Laser_Rojo,0);
             else if (*ptr_laser_sel == 0) digitalWrite(Laser_Verde,0);
          break;}
    }

}

//___________________________________________________________________________________________________________________
bool armado(int *ptr_laser_sel)
{
  //Posición inicial del sensor  CUANDO INICIA EL ENSAYO
   if (*ptr_laser_sel == 1)
   {
    pasos_p_inicializacion=pasos_p_inicializacion_experimentoA_laserVerde;// experimento A 
   }
   else
    {
      if (*ptr_laser_sel == 0)
      {
        pasos_p_inicializacion=pasos_p_inicializacion_experimentoB_laserRojo;// experimento B 
      }
    }

   long TimeOut = millis ();
   bool falla = false;
   digitalWrite(EN1,1); delay(5);
   digitalWrite(DIR1,1); delay(5);
   digitalWrite(EN1,0); delay(5); //Cambio de giro
   while (!(digitalRead(I_inicio)))
   {
         digitalWrite(STEP1,HIGH);
         delay(5);
         digitalWrite(STEP1,LOW);
         delay(5);    
         if (millis() > TimeOut + 30000) {falla = true; break;} // verifica que el fin de carrera fue presionado o que paso un timeout
   }
   if (falla){
    digitalWrite(EN1,1);
    return(false);
   }
   else
   {
     x_pos = 0;
     digitalWrite(DIR1,0);
     for (int j = 1; j < pasos_p_inicializacion; j++)
     {
       digitalWrite(STEP1,HIGH);
       delay(5);
       digitalWrite(STEP1,LOW);
       delay(5);    
     }
     digitalWrite(EN1,1);
     digitalWrite(Laser_Rojo,0);// apagado de laser
     digitalWrite(Laser_Verde,0);// apagado de laser
     return(true);
   }
}
