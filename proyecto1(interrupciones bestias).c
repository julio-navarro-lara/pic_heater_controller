#include "18F4520.H"
#include "proyecto1.h"
#fuses HS, NOPROTECT, BROWNOUT, PUT, NOLVP, NOXINST, WDT2048
//El watchdog-timer salta cada 8.192 segundos, aproximadamente
#use delay(clock=8000000, restart_wdt)
#use rs232(baud=4800, xmit=PIN_C6,rcv=PIN_C7)
#include <ctype.h>
#include <stdlib.h>
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

int8 temperatura_caldera_superada; //Control de la histéresis en la caldera
int8 temp_habitacion_superada;  //Control de la histéresis del ambiente
int8 encendido_por_alarma; //Indica si la última vez que se encendió fue en respuesta o no a una alarma
int8 toca_encender; //Indica si la próxima alarma es de encendido o de apagado
float termostato; //Temperatura límite de la vivienda
float termostato_provisional;
float temperatura; //Temperatura de la vivienda
float histeresis_vivienda = 0.5; //Intervalo de histéresis de la vivienda
char key; //Tecla a pulsar
char keys[2]; //Cadena para almacenar números como caracteres

//Estructura de un intervalo de programación
typedef struct
{
   unsigned int8 horas_inicio;
   unsigned int8 minutos_inicio;
   unsigned int8 horas_fin;
   unsigned int8 minutos_fin;
   unsigned int8 termostato;
}programacion;

//Vector que incluye todas las programaciones horarias
//La información se distribuye en grupos de 5: hora_inicio, minutos_inicio,
//hora_fin, minutos_fin y termostato.
programacion programaciones[5];
//Programacion en curso en este momento
programacion prg;
int8 num_intervalos;
int8 posicion_alarmas;


//Estructura para leer el tiempo del reloj
date_time_t tiempo;

//Registro de la hora en la que se enciende el sistema
int8 hora_encendido;
int8 minutos_encendido;

//Registro de la hora en la que se enciende la caldera
//Se supone que el encendido de la caldera se realiza en intervalos cortos,
//que se alcanza la temperatura deseada relativamente rápido.
int8 hora_caldera;
int8 minutos_caldera;
int8 segundos_caldera;
long t_total_caldera;

//Variables para el registro en memoria
unsigned long num_registros;
int8 anno_actual;
int8 anno_actual_1_to_3;



void representar(int *t, int tamanno);
int8 comprobar_temperatura();
int8 comp_caldera();
void mostrar_temperatura();
void encender_sistema();
void apagar_sistema();
void encender_caldera();
void apagar_caldera();
int8 buscar_numero();
int8 comprobar_hora(int8 hora1,int8 min1, int8 hora2, int8 min2);
void ordenar_programaciones();
long diferencia_tiempo(int8 hora1,int8 min1, int8 hora2, int8 min2);
long diferencia_tiempo_sec(int8 hora1,int8 min1, int8 sec1, int8 hora2, int8 min2, int8 sec2);
void seleccionar_alarma();
void programar_proxima_alarma();
void grabar_programaciones();
void leer_programaciones();
void representar_registros();

#INT_EXT
activacion_led(){
   int8 contador, contador2, valor;
   int8 horas_i, min_i, min_f, horas_f,terms; //BORRAR LUEGO
   long numero;
   //Variables provisionales para buscar en la lista de programaciones
   programacion pr, pr2;

   //Ponemos en key un caracter cualquiera para que no se corresponda
   //con ningún botón del teclado.
   key='J';
   setup_wdt(WDT_OFF);
   lcd_send_byte(LCD_ORDEN,LCD_CLEAR);
   printf(lcd_putc, "  Indique la \n  operacion");
   while(key!='1' && key!='2' && key!='3' && key!='4' && key!='N'){
      key=get_key();
   }

   //Se elige la opción
   switch(key)
   {
      case '1':

               if(!sistema_encendido)
               {
                  lcd_send_byte(LCD_ORDEN,LCD_CLEAR);
                  printf(lcd_putc, "  Encendiendo\n  sistema...");
                  encender_sistema();
               }else
               {
                  lcd_send_byte(LCD_ORDEN,LCD_CLEAR);
                  printf(lcd_putc, "  Apagando\n  sistema...");
                  apagar_sistema();
                  if(motor)
                     motor = 0;

                  //Si estaba encendido por alguna alarma debemos indicarlo
                  if(encendido_por_alarma)
                  {
                     termostato=termostato_provisional;
                     encendido_por_alarma = FALSE;
                  }
               }
               break;

      case '2':

               key = 'J';
               lcd_send_byte(LCD_ORDEN, LCD_CLEAR);
               if(temp_habitacion_superada)
                  printf(lcd_putc, "Modificar\ntermostato %.0f%cC",termostato+histeresis_vivienda,223);
               else
                  printf(lcd_putc, "Modificar\ntermostato %.0f%cC",termostato,223);
               delay_ms(LCD_T_RETARDO*2);

               lcd_send_byte(LCD_ORDEN, LCD_CLEAR);
               printf(lcd_putc, "Nuevo valor:\n       %cC",223);
               lcd_gotoxy(6,2);

               valor=buscar_numero();

               if(valor==100)
                  goto salir;
               else
               {
                  delay_ms(LCD_T_RETARDO);
                  lcd_send_byte(LCD_ORDEN, LCD_CLEAR);
                  printf(lcd_putc, "  Aplicando\n  cambios...");
                  if(valor<=temp_max && valor>=temp_min)
                  {
                     if(temp_habitacion_superada)
                        termostato = (float)valor - histeresis_vivienda;
                     else
                        termostato = (float)valor;

                     //Guardamos el nuevo valor del termostato
                     graba_ee(eeprom_termostato, 1, &valor);

                     //printf("%3.1f ",termostato);
                  }
                  else
                  {
                     lcd_send_byte(LCD_ORDEN, LCD_CLEAR);
                     printf(lcd_putc, "   Valor\n   incorrecto");
                     delay_ms(LCD_T_RETARDO);
                  }
               }

               break;

      case '3':
               key='J';
               lcd_send_byte(LCD_ORDEN, LCD_CLEAR);
               printf(lcd_putc," Programar\n sistema");
               delay_ms(LCD_T_RETARDO);

               for(contador=0; contador<5; contador++)
               {
                  lcd_send_byte(LCD_ORDEN, LCD_CLEAR);
                  printf(lcd_putc, " INTERVALO %i\n   :      :",contador+1);
                  lcd_gotoxy(2,2);
                  lcd_send_byte(LCD_ORDEN, LCD_CURSOR);

                  pr.horas_inicio = buscar_numero();

                  if(pr.horas_inicio==100)
                     goto salir;

                  if(pr.horas_inicio==101)
                  {
                     if(contador==0)
                     {
                        //Desactivamos las alarmas
                        PCF8583_write_byte(PCF8583_CTRL_STATUS_REG,
                              PCF8583_START_COUNTING);
                        goto salir;
                     }
                     else
                        break;
                  }


                  lcd_gotoxy(5,2);

                  pr.minutos_inicio = buscar_numero();

                  if(pr.minutos_inicio==100)
                     goto salir;

                  if(pr.minutos_inicio==101)
                  {
                     if(contador==0)
                     {
                        //Desactivamos las alarmas
                        PCF8583_write_byte(PCF8583_CTRL_STATUS_REG,
                              PCF8583_START_COUNTING);
                        goto salir;
                     }
                     else
                        break;
                  }


                  lcd_gotoxy(9,2);

                  pr.horas_fin = buscar_numero();

                  if(pr.horas_fin==100)
                     goto salir;

                  if(pr.horas_fin==101)
                  {
                     if(contador==0)
                     {
                        //Desactivamos las alarmas
                        PCF8583_write_byte(PCF8583_CTRL_STATUS_REG,
                              PCF8583_START_COUNTING);
                        goto salir;
                     }
                     else
                        break;
                  }


                  lcd_gotoxy(12,2);

                  pr.minutos_fin = buscar_numero();

                  if(pr.minutos_fin==100)
                     goto salir;

                  if(pr.minutos_fin==101)
                  {
                     if(contador==0)
                     {
                        //Desactivamos las alarmas
                        PCF8583_write_byte(PCF8583_CTRL_STATUS_REG,
                              PCF8583_START_COUNTING);
                        goto salir;
                     }
                     else
                        break;
                  }


                  lcd_send_byte(LCD_ORDEN, LCD_NO_CURSOR);

                  delay_ms(LCD_T_RETARDO);

                  if(pr.horas_inicio>23 || pr.horas_fin>23 || pr.minutos_inicio>59 || pr.minutos_fin>59
                        || comprobar_hora(pr.horas_fin,pr.minutos_fin,pr.horas_inicio,pr.minutos_inicio))
                  {
                     lcd_send_byte(LCD_ORDEN, LCD_CLEAR);
                     printf(lcd_putc, "   Valores\n   incorrectos");
                     delay_ms(LCD_T_RETARDO);
                     contador--;
                     continue;
                  }

                  if(diferencia_tiempo(pr.horas_inicio, pr.minutos_inicio, pr.horas_fin, pr.minutos_fin)>240)
                  {
                     lcd_send_byte(LCD_ORDEN, LCD_CLEAR);
                     printf(lcd_putc, "Intervalo limite\nde 4 horas");
                     delay_ms(LCD_T_RETARDO);
                     contador--;
                     continue;

                  }

                  lcd_send_byte(LCD_ORDEN, LCD_CLEAR);

                  printf(lcd_putc, "Termostato %i\n      %cC", contador+1, 223);

                  lcd_gotoxy(4,2);

                  lcd_send_byte(LCD_ORDEN, LCD_CURSOR);

                  pr.termostato = buscar_numero();

                  lcd_send_byte(LCD_ORDEN, LCD_NO_CURSOR);

                  delay_ms(LCD_T_RETARDO);

                  if(pr.termostato>temp_max || pr.termostato<temp_min)
                  {
                     lcd_send_byte(LCD_ORDEN, LCD_CLEAR);
                     printf(lcd_putc, "   Valor\n   incorrecto");
                     delay_ms(LCD_T_RETARDO);
                     contador--;
                     continue;
                  }

                  //Comprobamos que los intervalos sean correctos respecto a los otros
                  for(contador2=0; contador2<contador; contador2++)
                  {
                     pr2 = programaciones[contador2];

                     //Controlamos todas las posibilidades de solapamiento
                     if(
                        (comprobar_hora(pr2.horas_inicio, pr2.minutos_inicio, pr.horas_fin, pr.minutos_fin) &&
                        !comprobar_hora(pr2.horas_inicio, pr2.minutos_inicio, pr.horas_inicio, pr.minutos_inicio))

                        ||(comprobar_hora(pr2.horas_fin, pr2.minutos_fin, pr.horas_fin, pr.minutos_fin) &&
                          !comprobar_hora(pr2.horas_fin, pr2.minutos_fin, pr.horas_inicio, pr.minutos_inicio))

                        ||(comprobar_hora(pr.horas_inicio, pr.minutos_inicio, pr2.horas_fin, pr2.minutos_fin) &&
                          !comprobar_hora(pr.horas_inicio, pr.minutos_inicio, pr2.horas_inicio, pr2.minutos_inicio))

                        ||(comprobar_hora(pr.horas_fin, pr.minutos_fin, pr2.horas_fin, pr2.minutos_fin) &&
                          !comprobar_hora(pr.horas_fin, pr.minutos_fin, pr2.horas_inicio, pr2.minutos_inicio))
                     )
                     {
                        lcd_send_byte(LCD_ORDEN, LCD_CLEAR);
                        printf(lcd_putc, "Solapamiento\nde intervalos!");
                        delay_ms(LCD_T_RETARDO*2);
                        goto salir;
                     }
                  }

                  programaciones[contador]=pr;
               }

               //Habilitamos la alarma
               PCF8583_write_byte(PCF8583_CTRL_STATUS_REG,
                           PCF8583_ACTIVAR_ALARMA);

               num_intervalos = contador;

               for(contador=0; contador<num_intervalos; contador++)
               {
                  horas_i=programaciones[contador].horas_inicio;
                  min_i=programaciones[contador].minutos_inicio;
                  horas_f=programaciones[contador].horas_fin;
                  min_f=programaciones[contador].minutos_fin;
                  terms=programaciones[contador].termostato;

                  printf("%i:%i  %i:%i  %i%cC\r", horas_i, min_i, horas_f, min_f, terms,223);
               }

               printf("========================================\r");

               ordenar_programaciones();

               for(contador=0; contador<num_intervalos; contador++)
               {
                  horas_i=programaciones[contador].horas_inicio;
                  min_i=programaciones[contador].minutos_inicio;
                  horas_f=programaciones[contador].horas_fin;
                  min_f=programaciones[contador].minutos_fin;
                  terms=programaciones[contador].termostato;

                  printf("%i:%i  %i:%i  %i%cC\r", horas_i, min_i, horas_f, min_f, terms,223);
               }

               seleccionar_alarma();

               programar_proxima_alarma();

               toca_encender = TRUE;

               grabar_programaciones();

               break;

      case '4':
               lcd_send_byte(LCD_ORDEN, LCD_CLEAR);
               lcd_send_byte(LCD_ORDEN, LCD_CURSOR);
               printf(lcd_putc, "  CLAVE:\n  ");

               contador = 0;

               while(contador!=2)
               {
                  key = get_key();
                  if(isdigit(key)){
                     lcd_putc('*');
                     keys[contador] = key;
                     key = 'J';
                     contador++;
                  }
               }

               numero = atoi(keys);

               contador = 0;
               while(contador!=2)
               {
                  key = get_key();
                  if(isdigit(key)){
                     lcd_putc('*');
                     keys[contador] = key;
                     key = 'J';
                     contador++;
                  }
               }

               numero = numero*100 + atoi(keys);

               delay_ms(LCD_T_RETARDO);

               if(numero == clave)
               {
                  lcd_send_byte(LCD_ORDEN, LCD_CLEAR);
                  lcd_send_byte(LCD_ORDEN, LCD_NO_CURSOR);
                  printf(lcd_putc, "  Modo\n  revision");
                  representar_registros();


               }else
               {
                  lcd_send_byte(LCD_ORDEN, LCD_CLEAR);
                  lcd_send_byte(LCD_ORDEN, LCD_NO_CURSOR);
                  printf(lcd_putc, "  Clave\n  incorrecta");
                  delay_ms(LCD_T_RETARDO);
                  goto salir;
               }



               break;

      case 'N':
               salir:
               lcd_send_byte(LCD_ORDEN,LCD_CLEAR);
               lcd_send_byte(LCD_ORDEN,LCD_NO_CURSOR);
               printf(lcd_putc, "  Anulando\n  operacion...");
               delay_ms(LCD_T_RETARDO);
               break;

   }

   setup_wdt(WDT_ON);

}

#INT_EXT1
alarma(){
   int8 hora, minutos;
   float temp;

   //printf("%x\r", PCF8583_read_byte(PCF8583_CTRL_STATUS_REG));
   //printf("%x\r", PCF8583_read_byte(PCF8583_ALARM_CONTROL_REG));

   //Desactivamos el flag de la alarma
   PCF8583_write_byte(PCF8583_CTRL_STATUS_REG,
                           PCF8583_START_COUNTING);

   if(toca_encender)
   {
      if(!sistema_encendido)
      {
         //Actualizamos el termostato
         termostato_provisional = termostato;
         termostato = prg.termostato;

         toca_encender = FALSE;
         encendido_por_alarma = TRUE;
         encender_sistema();

         hora = prg.horas_fin;
         minutos = prg.minutos_fin;

         delay_ms(250);

         PCF8583_establecer_alarma(hora, minutos, 0);


      }else
      {
         delay_ms(250);

         //Si ya está encendido, nos vamos a la siguiente alarma
         posicion_alarmas++;
         if(posicion_alarmas==num_intervalos)
            posicion_alarmas = 0;
         programar_proxima_alarma();

         toca_encender = TRUE;

      }
   }else
   {
      delay_ms(250);

      if(sistema_encendido && encendido_por_alarma)
      {
         //Restauramos el valor del termostato que había antes de la alarma
         termostato=termostato_provisional;
         encendido_por_alarma = FALSE;
         apagar_sistema();
      }

      posicion_alarmas++;
      if(posicion_alarmas==num_intervalos)
         posicion_alarmas = 0;
      programar_proxima_alarma();

      toca_encender = TRUE;
   }

   //Activamos de nuevo la alarma
   PCF8583_write_byte(PCF8583_CTRL_STATUS_REG,
                           PCF8583_ACTIVAR_ALARMA);
}

//***********************************************************
main()
{
   int8 data[6];
   int8 contador, horas_i, horas_f, min_i, min_f, terms; //Borrar luego

   set_tris_b(0x03);
   set_tris_d(0x0F);

   //Mensaje de inicio
   lcd_init();
   lcd_send_byte(LCD_ORDEN, LCD_CLEAR);
   lcd_send_byte(LCD_ORDEN, LCD_NO_CURSOR);
   printf(lcd_putc, "Inicializando...");

   //Propiedades de conversión analógico digital
   setup_comparator(A0_A3_A1_A2);
   setup_adc_ports(AN0_TO_AN3);
   setup_adc(adc_clock_div_32);

   //Recuperación de datos de la memoria
   lee_ee(eeprom_termostato, 6, data);
   termostato = (float)data[0];
   num_intervalos = data[1];
   num_registros = (float)data[2]*256 + (float)data[3];
   anno_actual = data[4];
   anno_actual_1_to_3 = data[5];

   leer_programaciones();

   if(num_intervalos!=0)
   {
      seleccionar_alarma();
      programar_proxima_alarma();
      toca_encender = TRUE;
   }else
   {
      //No activamos las alarmas
      PCF8583_write_byte(PCF8583_CTRL_STATUS_REG,
                              PCF8583_START_COUNTING);
   }

   for(contador=0; contador<num_intervalos; contador++)
               {
                  horas_i=programaciones[contador].horas_inicio;
                  min_i=programaciones[contador].minutos_inicio;
                  horas_f=programaciones[contador].horas_fin;
                  min_f=programaciones[contador].minutos_fin;
                  terms=programaciones[contador].termostato;

                  printf("%i:%i  %i:%i  %i%cC\r", horas_i, min_i, horas_f, min_f, terms,223);
               }



   //Inicializamos salidas
   sistema_encendido = 0;
   motor = 0;
   caldera_encendida = 0;
   encendido_por_alarma = FALSE;
   t_total_caldera = 0;

   temperatura = ds1820_read();
   if(temperatura >= termostato)
   {
      temp_habitacion_superada = 1;
      termostato = termostato - histeresis_vivienda;
   }else
      temp_habitacion_superada = 0;

   mostrar_temperatura();


   if(!C1OUT)
      temperatura_caldera_superada = 1;
   else
      temperatura_caldera_superada = 0;

   enable_interrupts(INT_EXT);
   enable_interrupts(INT_EXT1);

   setup_wdt(WDT_ON);


   while(1){
      //Desactivamos interrupciones para que no se interrumpa el proceso de medida de temperatura.
      disable_interrupts(GLOBAL);
      //Si la temperatura de la caldera es menor que la necesaria, encendemos la caldera.
      if(C1OUT)
      {
         if(temperatura_caldera_superada)
         {
            if(!comp_caldera())
            {
               temperatura_caldera_superada = 0;
               encender_caldera();
            }

         }
         else if(!caldera_encendida)
         {
            encender_caldera();
         }
      }
      else
      {
         if(!temperatura_caldera_superada)
            temperatura_caldera_superada = 1;
         if(caldera_encendida)
            apagar_caldera();
      }


      if(comprobar_temperatura() && sistema_encendido && temperatura_caldera_superada)
      {
         if(!motor)
            motor = 1;

         //Comprobamos que el sistema no lleve encendido más de 4 horas
         //Ya leimos el tiempo en comprobar_temperatura()
         if(diferencia_tiempo(hora_encendido, minutos_encendido, tiempo.hours, tiempo.minutes)>240)
         {
            lcd_send_byte(LCD_ORDEN,LCD_CLEAR);
            printf(lcd_putc, "Demasiado tiempo\nencendido!");
            delay_ms(LCD_T_RETARDO);
            lcd_send_byte(LCD_ORDEN,LCD_CLEAR);
            printf(lcd_putc, "  Apagando\n  sistema...");
            apagar_sistema();

            if(motor)
              motor = 0;

            if(encendido_por_alarma)
            {
               termostato = termostato_provisional;
               encendido_por_alarma = FALSE;
            }

         }

      }
      else if(motor)
         motor = 0;

      enable_interrupts(GLOBAL);
      sleep();

   }





}


//Devuelve TRUE si la temperatura de la habitación es menor que la indicada
//en el termostato, y FALSE en caso contrario.
int8 comprobar_temperatura()
{
   temperatura = ds1820_read();

   mostrar_temperatura();

   if(temperatura < termostato)
   {
      if(temp_habitacion_superada)
      {
         temp_habitacion_superada = 0;
         termostato = termostato + histeresis_vivienda;
      }
      return 1;
   }
   else
   {
      if(!temp_habitacion_superada)
      {
         temp_habitacion_superada = 1;
         termostato = termostato - histeresis_vivienda;
      }
      return 0;
   }


}

//Devuelve TRUE si debemos apagar la caldera. Esta función tiene en cuenta
//cierta histéresis para evitar ciclos continuos de apagado-encendido.
int8 comp_caldera()
{
   //Suponemos una variación de la temperatura del agua en la caldera de 70ºC
   //a 25ºC, con una precisión que depende del potenciómetro.
   float termostato, temperatura;

   set_adc_channel(termostato_caldera);
   delay_us(10);
   termostato = t_min_caldera + ((t_max_caldera-t_min_caldera)*(float)read_adc())/AD_num_valores;

   set_adc_channel(temperatura_caldera);
   delay_us(10);
   temperatura = t_min_caldera + ((t_max_caldera-t_min_caldera)*(float)read_adc())/AD_num_valores;

   if(temperatura > termostato-histeresis_caldera)
      return 1;
   else
      return 0;
}

void mostrar_temperatura()
{
   PCF8583_read_datetime(&tiempo);

   lcd_send_byte(LCD_ORDEN, LCD_CLEAR);
   printf(lcd_putc, "TEMP - %3.1f %cC\nHORA - ", temperatura, 223);
   if(tiempo.hours < 10)
      printf(lcd_putc, "0");
   printf(lcd_putc, "%u:", tiempo.hours);
   if(tiempo.minutes < 10)
      printf(lcd_putc, "0");
   printf(lcd_putc, "%u", tiempo.minutes);
}

void encender_sistema()
{
   PCF8583_read_datetime(&tiempo);

   hora_encendido    = tiempo.hours;
   minutos_encendido = tiempo.minutes;

   t_total_caldera = 0;

   if(caldera_encendida)
   {
      hora_caldera = tiempo.hours;
      minutos_caldera = tiempo.minutes;
      segundos_caldera = tiempo.seconds;
   }

   sistema_encendido = 1;
}

void apagar_sistema()
{
   long minutos_trans;
   int t_entera, t_decimal;
   float temperatura;
   int data[8];

   //Si llegamos al límite de la memoria, se resetea
   if((eeprom_registros + num_registros*2*8)>=0x1000)
      num_registros = 0;

   PCF8583_read_datetime(&tiempo);

   minutos_trans = diferencia_tiempo(hora_encendido, minutos_encendido, tiempo.hours, tiempo.minutes);

   temperatura = ds1820_read();

   t_entera = (int)temperatura;
   t_decimal = (int)((temperatura-t_entera)*100);

   if(caldera_encendida)
      t_total_caldera = t_total_caldera + diferencia_tiempo_sec(hora_caldera, minutos_caldera, segundos_caldera, tiempo.hours, tiempo.minutes, tiempo.seconds);



   //Almacenamos todos los datos requeridos
   data[0] = tiempo.day;      //Día de desconexión
   data[1] = tiempo.month;    //Mes de desconexión
   data[2] = tiempo.hours;     //Hora de desconexión
   data[3] = tiempo.minutes;      //Minutos de desconexión
   data[4] = t_entera;        //Valor entero de última temperatura medida
   data[5] = t_decimal;       //Valor decimal de última temperatura medida
   data[6] = (int)minutos_trans;   //Minutos transcurridos desde el encendido
   data[7] = (int)(t_total_caldera/60); //Tiempo total que ha estado encendida la caldera
                                 //en este intervalo

   graba_ee(eeprom_registros + num_registros*2*8, 8, data);

   //Calculamos el año
   if(anno_actual_1_to_3 != tiempo.year)
   {
      anno_actual++;
      anno_actual_1_to_3++;

      data[0] = anno_actual;
      data[1] = anno_actual_1_to_3;

      graba_ee(eeprom_anno_actual, 2, data);
   }

   graba_ee(eeprom_registros + num_registros*2*8 + 8, 1, &anno_actual);

   num_registros++;

   data[0] = (int)(num_registros/256);
   data[1] = (int)(num_registros - data[0]*256);

   graba_ee(eeprom_num_registros, 2, data);


   sistema_encendido = 0;
}

void encender_caldera()
{
   PCF8583_read_datetime(&tiempo);

   hora_caldera = tiempo.hours;
   minutos_caldera = tiempo.minutes;
   segundos_caldera = tiempo.seconds;

   caldera_encendida = 1;

}

void apagar_caldera()
{
   int8 hora, minutos, segundos;
   PCF8583_read_datetime(&tiempo);

   hora = tiempo.hours;
   minutos = tiempo.minutes;
   segundos = tiempo.seconds;

   t_total_caldera = t_total_caldera + diferencia_tiempo_sec(hora_caldera, minutos_caldera, segundos_caldera, hora, minutos, segundos);

   caldera_encendida = 0;

}

//Escanea el teclado hasta encontrar un número de dos cifras o hasta que se decida salir
int8 buscar_numero(){
   int8 contador = 0;
   int8 numero;

   while(key != 'N' && key!='S' && contador!=2)
   {
      key = get_key();
      if(isdigit(key)){
         lcd_putc(key);
         keys[contador] = key;
         key = 'J';
         contador++;
      }
   }

   if(key == 'N')
      return 100;

   if(key == 'S')
      return 101;

   numero = atoi(keys);
   return numero;

}

//Devuelve true si hora1:min1 corresponde a un tiempo menor o igual que hora2:min2
int8 comprobar_hora(int8 hora1, int8 min1, int8 hora2, int8 min2)
{
   if(hora1 < hora2)
      return TRUE;
   if(hora1 > hora2)
      return FALSE;
   if(hora1==hora2)
   {
      if(min1 <= min2)
         return TRUE;
      else
         return FALSE;
   }
}

//Ordena las programaciones por orden de hora
void ordenar_programaciones()
{
   programacion *pr1;
   programacion *pr2;
   int8 contador1, contador2;
   int8 h_inicio_1, min_inicio_1, h_inicio_2, min_inicio_2;
   pr1 = programaciones;
   pr2 = programaciones;


   //Algoritmo de la burbuja
   for(contador1=0; contador1 < num_intervalos; contador1++)
   {
      pr1 = programaciones+num_intervalos-2;
      pr2 = programaciones+num_intervalos-1;

      for(contador2=0; contador2 < num_intervalos-contador1-1; contador2++)
      {
         h_inicio_1   = (*pr1).horas_inicio;
         min_inicio_1 = (*pr1).minutos_inicio;
         h_inicio_2   = (*pr2).horas_inicio;
         min_inicio_2 = (*pr2).minutos_inicio;
         if(comprobar_hora(h_inicio_2, min_inicio_2, h_inicio_1, min_inicio_1))
         {
            programacion temporal;
            temporal = *pr1;
            *pr1 = *pr2;
            *pr2 = temporal;
         }

         pr1--;
         pr2--;
      }

   }

}

//Calcula la diferencia de tiempo en minutos entre hora1:min1 y hora2:min2
//Si hora1:min1 es mayor que hora2:min2, los considera en días consecutivos
long diferencia_tiempo(int8 hora1, int8 min1, int8 hora2, int8 min2)
{
   long dif;
   if(comprobar_hora(hora1,min1,hora2,min2))
      dif = ((long)hora2*60 + (long)min2) - ((long)hora1*60 + (long)min1);
   else
      dif = (24*60-((long)hora1*60 + (long)min1)) + (long)hora2*60 + (long)min2;
   return dif;

}

//Calcula la diferencia de tiempo en segundos entre hora1:min1:sec1 y hora2:min2:sec2
//Si hora1:min1:sec1 es mayor que hora2:min2:sec2, los considera en días consecutivos
long diferencia_tiempo_sec(int8 hora1, int8 min1, int8 sec1, int8 hora2, int8 min2, int8 sec2)
{
   unsigned long dif;
   unsigned long dif2;
   if(comprobar_hora(hora1,min1,hora2,min2))
   {
      if(hora1==hora2 && min1==min2)
         dif = abs(sec2-sec1);
      else
      {
         dif2 = ((long)hora2*60 + (long)min2) - ((long)hora1*60 + (long)min1);
         //No hacemos la diferencia directamente porque el long no da de sí
         dif = dif2*60 + sec2 - sec1;
      }
   }
   else{
      dif2 = (24*60-((long)hora1*60 + (long)min1)) + (long)hora2*60 + (long)min2;
      dif = dif2*60 + sec2 - sec1;
   }
   return dif;

}

//Selecciona la alarma más cercana a la hora en la que nos encontramos
//Devuelve true si hay alarmas y false si no las hay
void seleccionar_alarma()
{
   int8 contador;
   int8 hora, minutos, hora_p, min_p;

   posicion_alarmas = 0;

   PCF8583_read_datetime(&tiempo);

   hora = tiempo.hours;
   minutos = tiempo.minutes;

   for(contador=0; contador<num_intervalos; contador++)
   {
       hora_p = programaciones[contador].horas_inicio;
       min_p  = programaciones[contador].minutos_inicio;
       if(!comprobar_hora(hora_p, min_p, hora, minutos))
       {
           posicion_alarmas = contador;
           break;
       }
   }


}

//Programa la próxima alarma que tendrá lugar
void programar_proxima_alarma()
{
   int8 hora, minutos;

   prg=programaciones[posicion_alarmas];
   hora = prg.horas_inicio;
   minutos = prg.minutos_inicio;

   PCF8583_establecer_alarma(hora, minutos, 0);
}

//Graba los datos de las programaciones en la memoria eeprom
void grabar_programaciones()
{
   int8 datos[5];
   programacion pr;
   int8 contador;

   for(contador=0; contador<num_intervalos; contador++)
   {
      pr = programaciones[contador];

      datos[0] = pr.horas_inicio;
      datos[1] = pr.minutos_inicio;
      datos[2] = pr.horas_fin;
      datos[3] = pr.minutos_fin;
      datos[4] = pr.termostato;

      graba_ee(eeprom_programaciones + (contador+1)*8, 5, datos);
   }

   graba_ee(eeprom_num_intervalos, 1, &num_intervalos);

   //He optado por escribir de programación en programación porque a pesar de que
   //en el datasheet aseguran que la memoria tiene una paginación de 32 bytes en la
   //simulación parece tener menos, así que de este modo nos evitamos problemas

}

//Recupera de la memoria los datos de las programaciones
void leer_programaciones(){
   int8 datos[5];
   programacion pr;
   int8 contador;

   //Se supone que antes ha sido recuperado el número de intervalos
   for(contador=0; contador<num_intervalos; contador++)
   {
      lee_ee(eeprom_programaciones + (contador+1)*8, 5, datos);

      pr.horas_inicio   = datos[0];
      pr.minutos_inicio = datos[1];
      pr.horas_fin      = datos[2];
      pr.minutos_fin    = datos[3];
      pr.termostato     = datos[4];

      programaciones[contador] = pr;

   }

}

//Representa la información de los registros en el puerto RS232
//Nunca guardamos la información de los registros para no saturar la memoria
//Aprovechamos la representación de los datos para calcular medias y demás y así
//no tener que realizar un segundo barrido de lectura en memoria.
void representar_registros()
{
   int8 contador, contador2;
   unsigned int8 data[8];
   int8 anno;
   float media_dia_on; //Tiempo medio en minutos que se enciende por día
   float media_dia_mes[12]; //Tiempos medios en minutos que se enciende en cada mes
   int dias_meses[12]; //Número de dias que tiene cada mes
   float media_dia_caldera; //Tiempo medio que se enciende la caldera
   float media_dia_mes_caldera[12]; //Tiempo medio que se enciende la caldera en cada mes

   //Capacidad para 4 años de datos
   float media_annos[4][12];
   float media_annos_caldera[4][12];
   int8 num_annos;
   int8 annos[4];

   //Para almacenar valores provisionales
   long tiempo_x_dia;
   long tiempo_x_dia_caldera;
   int8 dia[3];
   long num_dias;

   //dias_meses = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
   dias_meses[0] = dias_meses[2] = dias_meses[4] = dias_meses[6] = dias_meses[7] = dias_meses[9] = dias_meses[11] = 31;
   dias_meses[1] = 28;
   dias_meses[3] = dias_meses[5] = dias_meses[8] = dias_meses[10] = 30;

   dia[0] = dia[1] = dia[2] = 0;
   num_dias = 0;
   num_annos = 0;

   media_dia_on = 0;
   media_dia_caldera = 0;

   printf("REGISTRO DE EVENTOS\r");
   printf("*******************\r");
   printf("Momento de apagado     Temperatura        Minutos encendido       Minutos caldera\r");
   printf("=================================================================================\r");
   //Poner encabezamiento

   for(contador=0; contador<num_registros; contador++)
   {
      lee_ee(eeprom_registros + contador*2*8, 8, data);
      lee_ee(eeprom_registros + contador*2*8 + 8, 8, &anno);

      //Si cambiamos de día
      if(dia[0]!=data[0] || dia[1]!=data[1] || dia[2]!=anno)
      {
         //Lógicamente, no tenemos en cuenta los días en los que no se enciende:
         //días de verano, calurosos, etc. Esta información puede visualizarse en la media
         //por meses.
         //Guardamos los datos del día anterior si el día no es el primero
         if(dia[0]!=0)
         {
            media_dia_on = media_dia_on + (float)tiempo_x_dia;
            media_dia_mes[dia[1]-1] = media_dia_on;
            media_dia_caldera = media_dia_caldera + (float)tiempo_x_dia_caldera;
            media_dia_mes_caldera[dia[1]-1] = media_dia_caldera;
         }
         //La sumatoria podría alcanzar un valor muy alto. El sistema no está preparado para eso

         if(dia[2]!=anno)
         {
            //Comprobamos que no nos encontrabamos en el año introducido por defecto
            if(dia[2]!=0)
            {
               for(contador2=0; contador2<12; contador2++)
               {
                  media_dia_mes[contador2] = media_dia_mes[contador2]/(float)dias_meses[contador2];
                  media_dia_mes_caldera[contador2] = media_dia_mes_caldera[contador2]/(float)dias_meses[contador2];
                  media_annos[num_annos][contador2] = media_dia_mes[contador2];
                  media_annos_caldera[num_annos][contador2] = media_dia_mes_caldera[contador2];
               }

               annos[num_annos]=dia[2];

               num_annos++;
            }

            //comprobar bisiesto
            //En realidad, un año será bisiesto si es divisible por 4 y no lo es por 100,
            //excepto si este último lo es por 400. Sin embargo, dado que trabajamos con las
            //dos últimas cifras y que el próximo año divisible por 4 que no lo es por 100
            //es el 2100, mejor nos limitaremos a la divisibilidad por 4.
            if(anno%4 == 0)
               dias_meses[1] = 29;
            else
               dias_meses[1] = 28;
         }


         //Y modificamos la información del día
         dia[0] = data[0];
         dia[1] = data[1];
         dia[2] = anno;

         num_dias++;
         tiempo_x_dia = 0;
         tiempo_x_dia_caldera = 0;
      }

      tiempo_x_dia = tiempo_x_dia + data[6];
      tiempo_x_dia_caldera = tiempo_x_dia_caldera + data[7];

      if(data[0]<10)
         printf(" ");

      printf("%u/", data[0]);

      if(data[1]<10)
         printf("0");

      printf("%u/", data[1]);

      if(anno<10)
         printf("0");

      printf("%u  ", anno);

      if(data[2]<10)
         printf("0");

      printf("%u:", data[2]);

      if(data[3]<10)
         printf("0");

      printf("%u          ", data[3]);

      if(data[4]<10)
         printf(" ");

      printf("%u.", data[4]);

      if(data[5]<10)
         printf("0");

      printf("%u%cC                ", data[5], 223);

      if(data[6]<100)
         printf(" ");
      if(data[6]<10)
         printf(" ");

      printf("%u                   ", data[6]);

      if(data[7]<100)
         printf(" ");
      if(data[7]<10)
         printf(" ");

      printf("%u\r", data[7]);

   }

   //Debemos realizar la operación de guardado para el último día
   if(dia[0]!=0)
   {
      media_dia_on = media_dia_on + (float)tiempo_x_dia;
      media_dia_mes[dia[1]-1] = media_dia_on;
      media_dia_caldera = media_dia_caldera + (float)tiempo_x_dia_caldera;
      media_dia_mes_caldera[dia[1]-1] = media_dia_caldera;
   }
   //La sumatoria podría alcanzar un valor muy alto. El sistema no está preparado para eso
   //Comprobamos que no nos encontrabamos en el año introducido por defecto
   if(dia[2]!=0)
   {
      for(contador2=0; contador2<12; contador2++)
      {
         media_dia_mes[contador2] = media_dia_mes[contador2]/(float)dias_meses[contador2];
         media_dia_mes_caldera[contador2] = media_dia_mes_caldera[contador2]/(float)dias_meses[contador2];
         media_annos[num_annos][contador2] = media_dia_mes[contador2];
         media_annos_caldera[num_annos][contador2] = media_dia_mes_caldera[contador2];
      }

      annos[num_annos]=dia[2];

      num_annos++;
   }

   media_dia_on = media_dia_on/(float)num_dias;
   media_dia_caldera = media_dia_caldera/(float)num_dias;

   repetir:
   //Menú para mostrar los datos
   printf("=================================================================================\r");
   printf("¿Que operacion desea realizar?\r");
   printf("0. Salir\r");
   printf("1. Extraer tiempos medios por dia\r");
   printf("2. Extraer datos mensuales\r");
   printf("=================================================================================\r");

   while(key!='1' && key!='2' && key!='0'){
      key=get_key();
   }

   switch(key)
   {
      case '0':
               break;
      case '1':
               key = 'J';
               printf("\rTiempo medio de encendido del sistema por dia: %3.2f minutos\r\r", media_dia_on);
               printf("Tiempo medio de encendido de la caldera por dia: %3.2f minutos\r\r", media_dia_caldera);
               goto repetir;
               break;

      case '2':
               key = 'J';
               for(contador=0; contador<num_annos; contador++)
               {
                  printf("\r20");
                  if(annos[contador]<10)
                     printf("0");
                  printf("%u\r", annos[contador]);
                  printf("--------------------------------------------------------------------------------------------------------------------\r");
                  printf("              Tiempo medio de encendido del sistema (min/dia)      Tiempo medio de encendido de la caldera (min/dia)\r");
                  printf("ENERO                              %f                                                     %f\r", media_annos[contador][0], media_annos_caldera[contador][0]);
                  printf("FEBRERO                            %f                                                     %f\r", media_annos[contador][1], media_annos_caldera[contador][1]);
                  printf("MARZO                              %f                                                     %f\r", media_annos[contador][2], media_annos_caldera[contador][2]);
                  printf("ABRIL                              %f                                                     %f\r", media_annos[contador][3], media_annos_caldera[contador][3]);
                  printf("MAYO                               %f                                                     %f\r", media_annos[contador][4], media_annos_caldera[contador][4]);
                  printf("JUNIO                              %f                                                     %f\r", media_annos[contador][5], media_annos_caldera[contador][5]);
                  printf("JULIO                              %f                                                     %f\r", media_annos[contador][6], media_annos_caldera[contador][6]);
                  printf("AGOSTO                             %f                                                     %f\r", media_annos[contador][7], media_annos_caldera[contador][7]);
                  printf("SEPTIEMBRE                         %f                                                     %f\r", media_annos[contador][8], media_annos_caldera[contador][8]);
                  printf("OCTUBRE                            %f                                                     %f\r", media_annos[contador][9], media_annos_caldera[contador][9]);
                  printf("NOVIEMBRE                          %f                                                     %f\r", media_annos[contador][10], media_annos_caldera[contador][10]);
                  printf("DICIEMBRE                          %f                                                     %f\r", media_annos[contador][11], media_annos_caldera[contador][11]);
                  printf("--------------------------------------------------------------------------------------------------------------------\r");
               }

               goto repetir;
               break;



   }



}





