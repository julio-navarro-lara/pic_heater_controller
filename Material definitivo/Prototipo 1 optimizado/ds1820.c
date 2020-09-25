
/***************************************************/
/*Driver básico del termómetro/termostato ds1820.c */
//
//Realiza una secuencia completa de incialización, conversión
//de temperatura, y lectura del (único) sensor de temperatura
//con alimentación externa (no parásita).
//
/***************************************************/

float ds1820_read(int* dir)
{
 int8 busy=0, temp1, temp2;
 signed int16 temp3;
 float result;

 onewire_reset();     // Cada acceso al sensor debe iniciarse con reset
 onewire_write(0x55); //Instrucción MATCH ROM
 //Mandamos por el one wire la dirección dir de 64 bits
 onewire_write(dir[0]);
 onewire_write(dir[1]);
 onewire_write(dir[2]);
 onewire_write(dir[3]);
 onewire_write(dir[4]);
 onewire_write(dir[5]);
 onewire_write(dir[6]);
 onewire_write(dir[7]);
 onewire_write(0x44); //Orden de inicio de conversión de temperatura

 while (busy == 0) 	//el sensor mantiene la linea a 0 mientras realiza la conversión
  busy = onewire_read(); //y la deja a 1 al completarla

 onewire_reset();
 onewire_write(0x55); //Instrucción MATCH ROM
 onewire_write(dir[0]);
 onewire_write(dir[1]);
 onewire_write(dir[2]);
 onewire_write(dir[3]);
 onewire_write(dir[4]);
 onewire_write(dir[5]);
 onewire_write(dir[6]);
 onewire_write(dir[7]);
 onewire_write(0xBE); //Envío de la orden READ SCRATCHPAD (lectura de la memoria temporal)
 temp1 = onewire_read(); //lectura del LSB de la temperatura
 temp2 = onewire_read(); //lectura del MSB de la temperatura

 temp3 = make16(temp2, temp1); //
 result = (float) temp3 / 2.0;   //Cálculo para el DS18S20 con 0.5 grad C de resolución
// result = (float) temp3 / 16.0;  //Cálculo para el DS18B20 con 0.1 grad C de resolución

 delay_ms(200);
 return(result);
}

//Recoge las direcciones de dos dispositivos y las devuelve en los vectores de int pasados como
//argumento (cada instrucción ocupa 64 bits). Funciona también si hay más dispositivos conectados,
//pero sólo devuelve las direcciones de dos de ellos.
void ds1820_recoger_direcciones(int* dir1, int* dir2)
{
   int contador, contador2;
   int bit1, bit2;

   onewire_reset();
   onewire_write(0xF0); //Función ROM SEARCH

   //Recorremos el array de 8 ints que contiene la dirección
   for(contador=0; contador<8; contador++)
   {
      for(contador2=0; contador2<8; contador2++)
      {
        //Leemos el bit correspondiente de dirección de los dispositivos
        bit1 = ow_read_bit();

        //Leemos el segundo bit, que será el complemento del anterior si no hay conflictos
        bit2 = ow_read_bit();

        //Si los bits son complementarios, tenemos que todos los dispositivos tienen el mismo bit de
        //dirección en esa posición.
        if(bit1!=bit2)
        {
           shift_right(&(dir1[contador]),1,bit1); //Guardamos el resultado

           //Escribimos el bit de dirección para confirmar a los dispositivos la identificación
           ow_write_bit(bit1);
        }else
        {
           //Si no son complementarios, habrá conflicto, y habrá al menos un dispositivo con 0 en esa
           //posición y al menos uno con 1. Elegimos el que tiene el 1 enviando un 1 por la línea e
           //inhabilitándose los que tienen 0, que ya no contestarán a la función.
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
