
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

 while (busy == 0) 	//el sensor mantiene la linea a 0 mientras realiza la conversi�n
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

void ds1820_termostato(){
   int8 busy=0;

   onewire_reset();
   onewire_write(0xCC);
   onewire_write(0x44);

   while(busy == 0)
      busy = onewire_read();

   onewire_reset();
   onewire_write(0xEC); //Buscamos la alarma de alg�n dispositivo (funci�n ALARM SEARCH)

}

//////////////////////////////////////////////////////////////////////////////////////

