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
  double tiempo = millis();
  int pasos = 0;

void setup() {
  //Incialización de entrada/salida;
        //Pines para el control de LASER
            pinMode(Laser_Rojo,OUTPUT); digitalWrite(Laser_Rojo,LOW);
            pinMode(Laser_Verde,OUTPUT); digitalWrite(Laser_Verde,LOW);
    //Pines de control de motores
        //Motor de Pickup (Cabezal de adquisición de intensidad lumínica)
            pinMode(EN1,OUTPUT);
            pinMode(STEP1,OUTPUT);
            pinMode(DIR1,OUTPUT);
        //Motor de selección de blanco (Intercambio de diferentes elementos difractantes (Regillas, alambres, etc.))
            pinMode(EN2,OUTPUT);
            pinMode(STEP2,OUTPUT);
            pinMode(DIR2,OUTPUT);
        //Pines de Salida digitales
            pinMode(I_inicio,INPUT);    
            pinMode(I_final,INPUT);            
            pinMode(D_aux,INPUT);    
        //Selección de Laser
            pinMode(Laser_Verde,OUTPUT);
            pinMode(Laser_Rojo,OUTPUT);
        //Pines Entrada/salida digitales
            pinMode(D_aux,OUTPUT);    

 
   Serial.begin(9600);
   Serial.println("INTENSIDAD_DE_LUZ");
   digitalWrite(EN1,0);
   digitalWrite(DIR1,0);
}

void loop() {
if (digitalRead(I_inicio)) digitalWrite(DIR1,1); 
 if (digitalRead(I_final)) digitalWrite(DIR1,0); 
  digitalWrite(STEP1,HIGH);
  //digitalWrite(13,HIGH);
  delay(5);
  digitalWrite(STEP1,LOW);
  //digitalWrite(13,LOW);
  delay(5);
  digitalWrite(EN1,1);
  Serial.println((1024-analogRead(A_AUX1)));
  delay(100);
  digitalWrite(EN1,0);
}
