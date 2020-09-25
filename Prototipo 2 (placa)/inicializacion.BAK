//****************************************************************
//Programa de inicialización de memoria y reloj para el PROYECTO 1
//****************************************************************

#include "18F4520.H"
#include "proyecto1.h"
#fuses HS, NOPROTECT, BROWNOUT, PUT, NOLVP, NOXINST, WDT2048
#use delay(clock=8000000, restart_wdt)
#use rs232(baud=4800, xmit=PIN_C6,rcv=PIN_C7)

#include "PCF8583.c"
#include "LCDeasy.c"
#include "1wire.c"
#include "ds1820.c"
//#include <stdlib.h>


#use fast_io(A)
#use fast_io(B)
#use fast_io(C)
#use fast_io(D)





//=================================
void main()
{
   date_time_t tiempo;
   int termostato;
   int num_intervalos;
   int num_registros;
   int anno_actual;
   int anno_actual_0_to_3;

   //Establecemos el estado de los puertos como entradas o salidas
   set_tris_b(0x03);
   set_tris_d(0x03);
   set_tris_c(0x03);

   sistema_encendido = FALSE;
   motor = FALSE;
   caldera_encendida = FALSE;
   
   /*

   termostato = 20;
   num_intervalos = 0;
   num_registros = 0;
   anno_actual = 10;
   anno_actual_0_to_3 = 2;

   write_eeprom(eeprom_termostato, termostato);
   write_eeprom(eeprom_num_intervalos, num_intervalos);
   write_eeprom(eeprom_num_registros, num_registros);
   write_eeprom(eeprom_anno_actual, anno_actual);
   write_eeprom(eeprom_anno_0_to_3, anno_actual_0_to_3);
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
   
   ds1820_establecer_TH_TL((float)127.5, (float)20);
   
   while(1)
   {
      if(ds1820_termostato())
         sistema_encendido = TRUE;
      else
         sistema_encendido = FALSE;
   
   }
   

}
