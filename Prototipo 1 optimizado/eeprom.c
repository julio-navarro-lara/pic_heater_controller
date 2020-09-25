//******************************************************
//Programa para escritura y lectura de la memoria EEPROM
//******************************************************
//Author: Julio Navarro Lara


//Definimos las palabras de control, que constan de tres partes
//   - Los bits m�s altos de direccionamiento del dispositivo (fijos): 1010
//   - Los bits de direcci�n configurables A2A1A0: 111 (en este caso)
//   - El bit R/W', 1 para lectura y 0 para escritura
//NOTA: No es necesario definir las caracter�sticas de la comunicaci�n I2C porque se supone
//que ya han sido especificadas en el c�digo principal.
#define CONTROL_W 0b10101110
#define CONTROL_R 0b10101111

void graba_ee(long int dir, int tam, int *wdata)
{
   boolean ack;
   int i;
   //Escribimos la palabra de control
   do{
      i2c_start();
      ack=i2c_write(CONTROL_W);
   }while(ack); //Esperamos a que el dispositivo responda
   //Cuando est� listo, devuelve un ACK como 0
   //Mandamos la direcci�n en la que se quiere escribir
   i2c_write(dir>>8);
   i2c_write(dir & 255);

   //Escribimos los datos contenidos en wdata
   for(i=0;i<tam;i++){
      i2c_write(*wdata);
      wdata++;
   }

   //Finalizamos la comunicaci�n
   i2c_stop();
}

void lee_ee(long int dir, int tam, int8 *rdata)
{
   boolean ack;
   int i;
   //Comenzamos escribiendo la direcci�n que queremos consultar
   //al igual que ya hicimos con el m�todo de escritura
   do{
      i2c_start();
      ack=i2c_write(CONTROL_W);
   }while(ack); //Esperamos a que el dispositivo responda
   i2c_write(dir>>8);
   i2c_write(dir & 255);

   //Reiniciamos la comunicaci�n para leer los datos
   i2c_start();
   //Escribimos palabra de control de escritura
   i2c_write(CONTROL_R);

   //Leemos hasta que completemos el bus
   for(i=0; i<tam-1; i++){
      //Se responde en cada caso con ACK
      //(la funci�n i2c_read tiene su argumento al rev�s: 1 corresponde a
      //ACK y 0 a NACK, cuando en la l�nea un valor bajo es ACK y uno alto, NACK)
      *rdata=i2c_read(1);
      rdata++;
   }
   //Debemos terminar la lectura con un NACK
   *rdata=i2c_read(0);
   i2c_stop();
}
