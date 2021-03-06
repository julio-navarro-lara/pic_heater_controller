#include "18F4520.H"
#include "proyecto1.h"
#fuses HS, NOPROTECT, BROWNOUT, PUT, NOLVP, NOXINST, WDT2048
#use delay(clock=8000000, restart_wdt)
#use rs232(baud=4800, xmit=PIN_C6,rcv=PIN_C7)
#include <ctype.h>
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


float temperatura;
int8 activar_interrupciones;

void leer_temperatura();
void mostrar_lcd();
void representar(int *t, int tamanno);

#INT_EXT
activacion_led(){
   caldera_encendida = !caldera_encendida;
   lcd_send_byte(LCD_ORDEN,LCD_APAGAR);
}


main()
{
   int flag=0;

   char c;
char weekday[10];
date_time_t dt; //////// El tipo de estructura date_time_t
                      // est� definido en PCF8583.c

   int wdatos [4]={'p',58,92,3};
   int rdatos [4];


   //Establecemos entradas y salidas
   set_tris_a(0x2C);
   set_tris_d(0x7F);
   /*
   char key;
   lcd_init();
   enable_interrupts(INT_EXT);
   enable_interrupts(GLOBAL);
   //Configurado perro guardi�n para 8.192 segundos
   setup_wdt(WDT_ON);

   sleep();

   setup_wdt(WDT_OFF);



   motor=0; //Apagamos el motor

   caldera_encendida=1;

   leer_temperatura();
   mostrar_lcd();

   while(1){
      key=get_key();
      if(key){
         lcd_gotoxy(1,1);
         lcd_putc(key);
      }

   }
   */

   /*TESTEO DE TERMOSTATO
   caldera_encendida=0;
   ds1820_establecer_TH_TL(60,2);


   while(1){
        flag = ds1820_termostato();
        if(flag)
           caldera_encendida=1;
        else
           caldera_encendida=0;
   }
   */

   /*COMPARADOR AD
   setup_comparator(A0_A3_A1_A2);

   while(1){
      if(C1OUT)
         //Temperatura inicio > temp caldera. No podemos mover el agua
         //OJO! si son iguales tambi�n se toma esta opci�n
         caldera_encendida=1;
      else
         caldera_encendida=0;
   }
   */

/*
   graba_ee(8,4,wdatos);
   lee_ee(8,4,rdatos);
   representar(rdatos, sizeof(rdatos));

   lcd_init();

   enable_interrupts(INT_EXT);
   enable_interrupts(GLOBAL);

   while(1){
   }
*/


      dt.month   = 12;    // December
      dt.day     = 31;    // 31
      dt.year    = 06;    // 2006
      dt.hours   = 23;    // 23 hours (11pm in 24-hour time)
      dt.minutes = 59;    // 59 minutes
      dt.seconds = 50;    // 50 seconds
      dt.weekday = 0;     // 0 = Sunday, 1 = Monday, etc.

      //PCF8583_set_datetime(&dt); ////------------Puesta en fecha-hora

   PCF8583_init();

   lcd_init();
   lcd_send_byte(0, 1); //Borra LCD
// lcd_send_byte(0, 2); //Posici�n inicial


// Read the date and time from the PCF8583 and display
// it once per second (approx.) on the LCD.

while(1)
  {

   delay_ms(1000);
   lcd_send_byte(0,2);
   PCF8583_read_datetime(&dt); ////-------------Lectura del RTC

   strcpy(weekday, weekday_names[dt.weekday]);  // ptr=strncpy (s1, s2,  n)
                                                // Copy up to n characters s2->s1

   printf(lcd_putc, "%s, %2u/%2u/%2u \nHora: %u:%2u:%2u       ",
           weekday, dt.day, dt.month, dt.year,
           dt.hours, dt.minutes, dt.seconds);


   }

}

//Con este m�todo representamos cualquier vector de int dado su tama�o
void representar(int *t, int tamanno){
   int i=0;
   do{
      printf("%i ",*(t+i));
   }while(i++<(tamanno-1));
   printf("\r");
}

void leer_temperatura(){
   temperatura=ds1820_read();

}

void mostrar_lcd(){
   lcd_send_byte(0,1); //Borramos la pantalla
   lcd_gotoxy(1,1);
   printf(lcd_putc,"TEMP: %3.1f ", temperatura);
   lcd_putc(223);
   lcd_putc("C    ");
}
