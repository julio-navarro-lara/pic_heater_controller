
/***************************************************/
/*Driver b�sico del term�metro/termostato ds1820.c */
//
//Realiza una secuencia completa de incializaci�n, conversi�n
//de temperatura, y lectura del (�nico) sensor de temperatura
//con alimentaci�n externa (no par�sita).
//
/***************************************************/

float ds1820_read()
{
 int8 busy=0, temp1, temp2;
 signed int16 temp3;
 float result;

 onewire_reset();     // Cada acceso al sensor debe iniciarse con reset
 onewire_write(0xCC); //Instrucci�n SKIP ROM (selecciona todos los dispositivos del 1wire)
 onewire_write(0x44); //Orden de inicio de conversi�n de temperatura

 while (busy == 0)    //el sensor mantiene la linea a 0 mientras realiza la conversi�n
  busy = onewire_read(); //y la deja a 1 al completarla

 onewire_reset();
 onewire_write(0xCC);
 onewire_write(0xBE); //Env�o de la orden READ SCRATCHPAD (lectura de la memoria temporal)
 temp1 = onewire_read(); //lectura del LSB de la temperatura
 temp2 = onewire_read(); //lectura del MSB de la temperatura

 temp3 = make16(temp2, temp1); //
 result = (float) temp3 / 2.0;   //Calculation for DS18S20 with 0.5 deg C resolution
// result = (float) temp3 / 16.0;  //Calculation for DS18B20 with 0.1 deg C resolution

 delay_ms(200);
 return(result);
}

//Establece la temperatura m�xima y m�nima para el termostato
void ds1820_establecer_TH_TL(float th, float tl){
   float th2, tl2;
   onewire_reset();
   onewire_write(0xCC); //Accedemos a todos los dispositivos
   onewire_write(0x4E); //Env�o de la orden Write Scratchpad (escritura en la memoria principal)
   th2=2*th;
   tl2=2*tl;
   onewire_write((int8)th2);
   onewire_write((int8)tl2);

   //Copiamos los valores en la ROM
   onewire_reset();
   onewire_write(0xCC);
   onewire_write(0x48);

   //Dejamos un poco de tiempo para que se escriba en la EEPROM
   //El tiempo ha sido calculado a partir de la simulaci�n en Proteus
   delay_us(10600);

}

//Intenta buscar la alarma del �nico sensor conectado por si ha saltado
//el termostato. Si lo ha hecho, devuelve TRUE, y en caso contrario, FALSE.
int1 ds1820_termostato(){
   int busy=0;
   int1 bit1, bit2;

   //Primero forzamos la conversi�n de la temperatura
   onewire_reset();
   onewire_write(0xCC);
   onewire_write(0x44);

   while(busy == 0)
      busy = onewire_read();

   onewire_reset();
   onewire_write(0xEC); //Buscamos la alarma de alg�n dispositivo (funci�n ALARM SEARCH)

   //Leemos dos bits consecutivos. Si el dispositivo tiene el flag de alarma activado, escribir�
   //en la l�nea el bit m�s bajo de su direcci�n ROM y, despu�s, su complemento. Si no, no har�
   //nada. Por eso, si ambos bits le�dos son diferentes, la alarma estar� activada, mientras que
   //si son iguales, no lo estar�n. No es necesario continuar leyendo toda la direcci�n del dispositivo.
   output_low(ONE_WIRE_PIN);
   delay_us( 2 );
   output_float(ONE_WIRE_PIN);
   delay_us( 8 );
   bit1 = input(ONE_WIRE_PIN); //Leemos el bit menos significativo de la direcci�n ROM
   delay_us( 120 );

   output_low(ONE_WIRE_PIN);
   delay_us( 2 );
   output_float(ONE_WIRE_PIN);
   delay_us( 8 );
   bit2 = input(ONE_WIRE_PIN); //Leemos su complemento (si la alarma est� activada)
   delay_us( 120 );

   if(bit1 != bit2)
      return TRUE;
   else
      return FALSE;
}

