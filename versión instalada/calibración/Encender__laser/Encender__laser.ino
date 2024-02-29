//---------------------------------------------------------------------------------------------------------------------- 
//Declarando pines de entrada/salida
    //Pines de control de motores
        //Motor de Pickup (Cabezal de adquisición de intensidad lumínica)
            #define EN1   10
            #define STEP1 11
            #define DIR1  12
        //Motor de selección de blanco (Intercambio de diferentes elementos difractantes (Regillas, alambres, etc.))
            #define EN2   6
            #define STEP2 7
            #define DIR2  8
    //Pines de Salida digitales
          //Selección de Laser
            #define Laser_Verde A2 //ANTES 5
            #define Laser_Rojo  6
    //Pines Entrada/salida digitales
            #define I_inicio  2    
            #define I_final  3            
            #define D_aux  4    
    //Entradas analógicas
            #define Int_Luz  A0
            #define A_AUX1   A1
            #define A_AUX2   A2
            #define A_AUX3   A3
//---------------------------------------------------------------------------------------------------------------------- 

//Variables:

void setup() {
  //Incialización de entrada/salida;
        //Pines para el control de LASER
            pinMode(Laser_Rojo,OUTPUT); digitalWrite(Laser_Rojo,HIGH);
            pinMode(Laser_Verde,OUTPUT); digitalWrite(Laser_Verde,HIGH);
}

void loop() {
}
