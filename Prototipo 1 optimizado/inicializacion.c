//****************************************************************
//Programa de inicialización de memoria y reloj para el PROYECTO 1
//****************************************************************

#include "18F4520.H"
#include "proyecto1.h"
#fuses HS, NOPROTECT, BROWNOUT, PUT, NOLVP, NOXINST, WDT2048, NOWDT
#use delay(clock=8000000, restart_wdt)
#use rs232(baud=4800, xmit=PIN_C6,rcv=PIN_C7)

#include "PCF8583.c"
#include "LCDeasy.c"
#include "1wire.c"
#include "ds1820.c"
#include "teclado.c"
#include "eeprom.c"


#use fast_io(A)
#use fast_io(B)
#use fast_io(C)
#use fast_io(D)





//=================================
void main()
{

   date_time_t tiempo;
   signed int termostato;
   int8 num_intervalos;
   long num_registros;
   int8 anno_actual;
   int8 anno_actual_1_to_3;
   int8 data[6];
   //Borrar estas líneas después
   float temperatura;
   int8 count;
   int8 m;
   int1 bit;
   int dir1[8], dir2[8];

   set_tris_b(0x03);
   set_tris_d(0x0F);

   sistema_encendido = 0;
   motor = 0;
   caldera_encendida = 0;
   /*
   termostato = 20;
   num_intervalos = 0;
   num_registros = 0;
   anno_actual = 9;
   anno_actual_1_to_3 = 1;

   data[0] = termostato;
   data[1] = num_intervalos;
   data[2] = (int)(num_registros/256);
   data[3] = (int)(num_registros - data[0]*256);
   data[4] = anno_actual;
   data[5] = anno_actual_1_to_3;

   graba_ee(eeprom_termostato, 6, data);
   */
   /*
   tiempo.month   = ;
   tiempo.day     = ;
   tiempo.year    = ;
   tiempo.hours   = ;
   tiempo.minutes = ;
   tiempo.seconds = ;
   tiempo.weekday = ;

   PCF8583_set_datetime(&tiempo);
   */


   /*
   ds1820_recoger_direcciones(dir1, dir2);

   for(termostato=7; termostato>=0; termostato--)
   {
      printf("%X", dir1[termostato]);
   }

   printf("\r");

   for(termostato=7; termostato>=0; termostato--)
   {
      printf("%X", dir2[termostato]);
   }

   printf("\r*************\r");



    ds1820_recoger_direcciones(dir1, dir2);

   for(termostato=7; termostato>=0; termostato--)
   {
      printf("%X", dir1[termostato]);
   }

   printf("\r");

   for(termostato=7; termostato>=0; termostato--)
   {
      printf("%X", dir2[termostato]);
   }

   printf("\r*************\r");

   printf("%3.2f\r", ds1820_read(dir1));
   printf("%3.2f\r", ds1820_read(dir2));

   
   ds1820_establecer_TH_TL(127.5, (float)20);

   while(1)
   {
      if(ds1820_termostato(dir1))
         sistema_encendido = TRUE;
      else
         sistema_encendido = FALSE;

      if(ds1820_termostato(dir2))
         caldera_encendida = TRUE;
      else
         caldera_encendida = FALSE;

   }
   */

}
