#include "18F4520.H"
#include "proyecto1.h"
#fuses HS, NOPROTECT, BROWNOUT, PUT, NOLVP, NOXINST, WDT2048
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

float temperatura;
int8 activar_interrupciones;
char key;

#INT_EXT
interrupcion(){
   int8 i;
   i=0;
   setup_wdt(WDT_OFF);

   lcd_send_byte(LCD_ORDEN,LCD_CLEAR);

   printf(lcd_putc, "  Indique la \n  operacion");

   //while(key!='1' && key!='2' && key!='3' && key!='4' && key!='N'){
   while(i<100){
      key=get_key();
      i++;
   }


   lcd_send_byte(LCD_ORDEN,LCD_CLEAR);
   setup_wdt(WDT_ON);
}

void leer_temperatura();
void mostrar_lcd();
void representar(int *t, int tamanno);


//=================================
void main()
{
   date_time_t tiempo;

   enable_interrupts(INT_EXT);
   enable_interrupts(GLOBAL);

   set_tris_b(0x03);
   set_tris_d(0x0F);

   sistema_encendido = 0;
   motor = 0;
   caldera_encendida = 0;

   lcd_init();
   //lcd_send_byte(LCD_ORDEN, LCD_NO_CURSOR);

   PCF8583_read_datetime(&tiempo);

   printf("%i ", tiempo.year);

   while(1){

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

