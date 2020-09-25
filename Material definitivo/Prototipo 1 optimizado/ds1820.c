
/***************************************************/
/*Driver b�sico del term�metro/termostato ds1820.c */
//
//Realiza una secuencia completa de incializaci�n, conversi�n
//de temperatura, y lectura del (�nico) sensor de temperatura
//con alimentaci�n externa (no par�sita).
//
/***************************************************/

float ds1820_read(int* dir)
{
 int8 busy=0, temp1, temp2;
 signed int16 temp3;
 float result;

 onewire_reset();     // Cada acceso al sensor debe iniciarse con reset
 onewire_write(0x55); //Instrucci�n MATCH ROM
 //Mandamos por el one wire la direcci�n dir de 64 bits
 onewire_write(dir[0]);
 onewire_write(dir[1]);
 onewire_write(dir[2]);
 onewire_write(dir[3]);
 onewire_write(dir[4]);
 onewire_write(dir[5]);
 onewire_write(dir[6]);
 onewire_write(dir[7]);
 onewire_write(0x44); //Orden de inicio de conversi�n de temperatura

 while (busy == 0) 	//el sensor mantiene la linea a 0 mientras realiza la conversi�n
  busy = onewire_read(); //y la deja a 1 al completarla

 onewire_reset();
 onewire_write(0x55); //Instrucci�n MATCH ROM
 onewire_write(dir[0]);
 onewire_write(dir[1]);
 onewire_write(dir[2]);
 onewire_write(dir[3]);
 onewire_write(dir[4]);
 onewire_write(dir[5]);
 onewire_write(dir[6]);
 onewire_write(dir[7]);
 onewire_write(0xBE); //Env�o de la orden READ SCRATCHPAD (lectura de la memoria temporal)
 temp1 = onewire_read(); //lectura del LSB de la temperatura
 temp2 = onewire_read(); //lectura del MSB de la temperatura

 temp3 = make16(temp2, temp1); //
 result = (float) temp3 / 2.0;   //C�lculo para el DS18S20 con 0.5 grad C de resoluci�n
// result = (float) temp3 / 16.0;  //C�lculo para el DS18B20 con 0.1 grad C de resoluci�n

 delay_ms(200);
 return(result);
}

//Recoge las direcciones de dos dispositivos y las devuelve en los vectores de int pasados como
//argumento (cada instrucci�n ocupa 64 bits). Funciona tambi�n si hay m�s dispositivos conectados,
//pero s�lo devuelve las direcciones de dos de ellos.
void ds1820_recoger_direcciones(int* dir1, int* dir2)
{
   int contador, contador2;
   int bit1, bit2;

   onewire_reset();
   onewire_write(0xF0); //Funci�n ROM SEARCH

   //Recorremos el array de 8 ints que contiene la direcci�n
   for(contador=0; contador<8; contador++)
   {
      for(contador2=0; contador2<8; contador2++)
      {
        //Leemos el bit correspondiente de direcci�n de los dispositivos
        bit1 = ow_read_bit();

        //Leemos el segundo bit, que ser� el complemento del anterior si no hay conflictos
        bit2 = ow_read_bit();

        //Si los bits son complementarios, tenemos que todos los dispositivos tienen el mismo bit de
        //direcci�n en esa posici�n.
        if(bit1!=bit2)
        {
           shift_right(&(dir1[contador]),1,bit1); //Guardamos el resultado

           //Escribimos el bit de direcci�n para confirmar a los dispositivos la identificaci�n
           ow_write_bit(bit1);
        }else
        {
           //Si no son complementarios, habr� conflicto, y habr� al menos un dispositivo con 0 en esa
           //posici�n y al menos uno con 1. Elegimos el que tiene el 1 enviando un 1 por la l�nea e
           //inhabilit�ndose los que tienen 0, que ya no contestar�n a la funci�n.
           shift_right(&(dir1[contador]),1,1);

           ow_write_bit(1);
        }
      }
   }

   //Repetimos exactamente el mismo proceso pero eligiendo en los conflictos el dispositivo con 0.
   onewire_reset();
   onewire_write(0xF0);

   for(contador=0; contador<8; contador++)
   {
      for(contador2=0; contador2<8; contador2++)
      {
        bit1 = ow_read_bit();

        bit2 = ow_read_bit();

        if(bit1!=bit2)
        {
           shift_right(&(dir2[contador]),1,bit1);

           ow_write_bit(bit1);
        }else
        {
           shift_right(&(dir2[contador]),1,0);

           ow_write_bit(0);
        }
      }
   }

}
