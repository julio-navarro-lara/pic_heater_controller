//*****************************************************************
//***   PROYECTO 1: Control de un sistema de calefacción de una ***
//***   vivienda utilizando un microcontrolador PIC16F887       ***
//*****************************************************************
//Author: Julio Navarro Lara        2010

//En este caso está preparado para ajustarse a un pic de gama media



#include "16F887.H"
#include "proyecto1.h"
#fuses HS, BROWNOUT, PUT, NOWDT
//El watchdog-timer salta cada 16.384 segundos, aproximadamente
#use delay(clock=8000000, restart_wdt)
#use rs232(baud=4800, xmit=PIN_C6,rcv=PIN_C7,RESTART_WDT)
#use i2c(master, sda=PIN_C4, scl=PIN_C3, NOFORCE_SW)
#include <ctype.h>
#include <stdlib.h>
#include "PCF8583.c"
#include "LCDeasy.c"
#include "1wire.c"
#include "ds1820.c"
#include "eeprom.c"


#use fast_io(A)
#use fast_io(B)
#use fast_io(C)
#use fast_io(D)

int1 temperatura_caldera_superada; //Control de la histéresis en la caldera
int1 temp_habitacion_superada;  //Control de la histéresis del ambiente
int1 encendido_por_alarma; //Indica si la última vez que se encendió fue en respuesta o no a una alarma
int1 toca_encender; //Indica si la próxima alarma es de encendido o de apagado
float termostato; //Temperatura límite de la vivienda
float termostato_provisional; //Valor de termostato provisional para cuando saltan las alarmas
float temperatura; //Temperatura de la vivienda
float histeresis_vivienda = 0.5; //Intervalo de histéresis de la vivienda

//Estructura de un intervalo de programación
typedef struct
{
   unsigned int horas_inicio;    //Hora a la que se inicia el encendido
   unsigned int minutos_inicio;  //
   unsigned int horas_fin;       //Hora a la que se apaga el sistema
   unsigned int minutos_fin;     //
   unsigned int termostato;      //Termostato vigente en ese intervalo
}programacion;

//Única programación horaria posible
//La información se distribuye en grupos de 5: hora_inicio, minutos_inicio,
//hora_fin, minutos_fin y termostato.
programacion prg;

//Vector para guardar datos en memoria
int data[8];

//Estructura para leer el tiempo del reloj
date_time_t tiempo;

//Registro de la hora en la que se enciende el sistema
int hora_encendido;
int minutos_encendido;

//Registro de la hora en la que se enciende la caldera
//Se supone que el encendido de la caldera se realiza en intervalos cortos,
//que se alcanza la temperatura deseada relativamente rápido.
int hora_caldera;
int minutos_caldera;
int segundos_caldera;
//Tiempo total de encendido de la caldera en cada intervalo de encendido del sistema
long t_total_caldera;

//Variables para el registro en memoria
unsigned long num_registros;  //Número de registros de fechas almacenados
int anno_actual;             //Año en el que estamos (de 0 a 99)
int anno_actual_0_to_3;      //Año en el que estamos (de 0 a 3)

//Flag para activar las interrupciones de interfaz con el teclado y alarma
int1 lcd_int;
int1 alarma_int;

//Variables para gestionar las alarmas por software
int hora_alarma;
int minutos_alarma;
int1 alarma;


//Listado de métodos utilizados ordenados por orden alfabético
//************************************************************
//Mostramos aquí una breve explicación de cada uno. El interior de los métodos está
//descrito después del método main.

//apagar_caldera() -> Apaga la caldera, almacenando el tiempo que ha estado encendida.
void apagar_caldera();
//apagar_sistema() -> Realiza todas las operaciones necesarias para el apagado
//del sistema, como el almacenamiento de los datos necesarios en memoria.
void apagar_sistema();
//buscar_numero(int led_unidades, int min, int max, int valor_inicial) -> Busca un número introducido con los
//botones + y - y se va mostrando el valor en la posición led_unidades del display (siempre en la fila 2). El
//rango de variación del valor está limitado por el intervalo [min,max]. Valor_inicial es el valor con el que
//se muestra la variable al principio. Si se pulsa SI, se aprueba el valor que aparece.
int buscar_numero(int led_unidades, int min, int max, int valor_inicial);
//buscar_numero_rs232() -> Registra la introducción de un número de dos cifras mediante la interfaz RS232 y
//lo devuelve. Además, si se pulsa el "backspace" durante el proceso, devuelve el código correspondiente a NO
int buscar_numero_rs232();
//comp_caldera() -> Devuelve TRUE si debemos apagar la caldera. Esta función tiene en cuenta
//cierta histéresis para evitar ciclos continuos de apagado-encendido.
int comp_caldera();
//comprobar_temperatura() -> Devuelve TRUE si la temperatura de la habitación es menor
//que la indicada en el termostato, y FALSE en caso contrario.
int comprobar_temperatura();
//diferencia_tiempo_sec(int hora1, int min1, int sec1, int hora2, int min2, int sec2) ->
//Calcula la diferencia de tiempo en segundos entre hora1:min1:sec1 y hora2:min2:sec2
//Si hora1:min1:sec1 es mayor que hora2:min2:sec2, los considera en días consecutivos
long diferencia_tiempo_sec(int hora1,int min1, int sec1, int hora2, int min2, int sec2);
//encender_caldera() -> Enciende la caldera, almacenando el tiempo de encendido en
//las variables correspondientes.
void encender_caldera();
//encender_sistema() -> Realiza todas las operaciones necesarias para el encendido
//del sistema, como registro del tiempo de inicio.
void encender_sistema();
//inicializacion() -> Gestiona la inicialización del sistema por parte del técnico.
void inicializacion();
//mostrar_temperatura() -> Muestra en la pantalla LCD la temperatura actual y la hora.
void mostrar_temperatura();
//representar_registros() -> Representa la información de los registros en el puerto RS232
//Nunca guardamos la información de los registros para no saturar la memoria y
//aprovechamos la representación de los datos para calcular medias y demás y así
//no tener que realizar un segundo barrido de lectura en memoria.
void representar_registros();

//--------------------------------------------------------------------------------

//Gestión de interrupciones de activación de la interfaz de usuario
#INT_EXT
void activacion_led(){
   //Se activa el flag correspondiente
   lcd_int = TRUE;
}

//***********************************************************
//Método main
//***********************************************************
void main()
{
   //Variable auxiliar
   int aux;
   
   //Configuramos el perro guardián
   setup_wdt(WDT_18MS | WDT_TIMES_1024);

   //Inicializamos salidas
   //Comenzamos con todo apagado (sistema, motor y caldera)
   sistema_encendido = FALSE;
   motor = FALSE;
   caldera_encendida = FALSE;
   encendido_por_alarma = FALSE;
   t_total_caldera = 0; //El tiempo total de encendido de la caldera también lo ponemos a 0

   //Establecemos el estado de los puertos como entradas o salidas
   set_tris_b(0x03);
   set_tris_d(0x00);
   set_tris_c(0xE7);

   //Inicializamos la pantalla lcd
   lcd_init();

   //No mostramos mensaje de inicio para ahorrar memoria

   //-----------------------------------------------------------
   //Rutina de inicialización del sistema por parte del técnico
   //-----------------------------------------------------------
   inicializacion();

   //Propiedades de conversión analógico digital
   //Caracterizamos los puertos A0 a A3 como entradas analógicas
   setup_adc_ports(sAN2 | sAN3);
   //Definimos el reloj de conversión
   setup_adc(adc_clock_div_32);

   //Leemos la temperatura
   temperatura = ds1820_read();
   if(!ds1820_termostato())
   {
      //Si supera el valor del termostato, lo indicamos con el flag
      temp_habitacion_superada = TRUE;
      //Tendremos que establecer un nuevo termostato teniendo en cuenta la histéresis de la temperatura
      termostato = termostato - histeresis_vivienda;
      //Guardamos el dato en el sensor
      //La TH la configuramos con un valor muy alto (127.5ºC)
      ds1820_establecer_TH_TL(127.5, termostato);
   }else
      //Si no se supera, se indica también
      temp_habitacion_superada = FALSE;


   //Asumimos que en principio la temperatura de la caldera no ha sido superada
   temperatura_caldera_superada = FALSE;
   if(comp_caldera())
      //Si la temperatura del agua en la caldera supera el termostato, activamos el flag
      temperatura_caldera_superada = TRUE;

   //Desactivamos los flag de las interrupciones
   alarma_int = lcd_int = FALSE;

   //Activamos las interrupciones del puerto B1
   enable_interrupts(INT_EXT);
   enable_interrupts(GLOBAL);

   //Repetimos la siguiente rutina hasta que se desconecte el micro
   while(1){
      inicio:
      //Inhabilitamos el perro guardián para que no interfiera con las gestión de interrupciones y
      //con la medida de la temperatura
      setup_wdt(WDT_OFF);

      //Si la interrupción de alarma se activa, gestionamos el proceso. En realidad no se trata de una
      //interrupción propiamente dicha, pues se activa por software según lo que apunto en el comentario
      //correspondiente a la activación (al final del main).
      if(alarma_int)
      {

         //Desactivamos interrupciones
         disable_interrupts(GLOBAL);

         if(toca_encender)
         {
            if(!sistema_encendido)
            {
               //Ejecutamos esto si se trata de una alarma de encendido y el sistema
               //está apagado.
               //Actualizamos el termostato, guardando el valor que había ya (teniendo
               //en cuenta la histéresis)
               if(temp_habitacion_superada)
                  termostato_provisional = termostato + histeresis_vivienda;
               else
                  termostato_provisional = termostato;
               termostato = prg.termostato;
               //Grabamos el termostato en el sensor como valor TL
               ds1820_establecer_TH_TL(127.5, termostato);

               //Si la temperatura no está superada, desactivamos el flag por si acaso
               //lo estaba con el otro valor de termostato.
               if(ds1820_termostato())
                  temp_habitacion_superada = FALSE;

               //Determinamos la próxima alarma como de apagado
               toca_encender = FALSE;

               //Activamos el flag de sistema encendido por alarma
               encendido_por_alarma = TRUE;
               //No mostramos mensaje para ahorrar memoria
               //Encendemos el sistema
               encender_sistema();

               //Recogemos el tiempo en el que debe de saltar la alarma de apagado.
               
               hora_alarma = prg.horas_fin;
               minutos_alarma = prg.minutos_fin;


            }else
            {
               //Si se trata de una alarma de encendido y el sistema está encendido
               delay_ms(250); //Retraso para evitar solapamiento de alarmas

               //Programamos la próxima alarma de encendido
               hora_alarma = prg.horas_inicio;
               minutos_alarma = prg.minutos_inicio;

               //Determinamos que la siguiente es de encendido
               toca_encender = TRUE;
            }
         }else
         {
            //Si se trata de una alarma de apagado

            delay_ms(250); //Retraso para evitar el solapamiento de alarmas

            if(sistema_encendido && encendido_por_alarma)
            {
               //Si el sistema está encendido y se trata de un encendido por alarma:
               //Restauramos el valor del termostato que había antes de la alarma
               termostato=termostato_provisional;
               //Grabamos la información en el sensor
               ds1820_establecer_TH_TL(127.5, termostato);
               //Determinamos que la próxima vez que se encienda no sea por alarma
               encendido_por_alarma = FALSE;
               //No mostramos mensaje para ahorrar memoria
               //Apagamos el sistema
               apagar_sistema();
            }

            //Reestablecemos la hora de la alarma
            hora_alarma = prg.horas_inicio;
            minutos_alarma = prg.minutos_inicio;

            //Y activamos que, efectivamente, se trata de una alarma de encendido
            toca_encender = TRUE;
         }

         //Activamos de nuevo la alarma
         alarma = TRUE;

         //Desactivamos el flag de interrupción
         alarma_int = FALSE;

         //Activamos de nuevo las interrupciones
         enable_interrupts(GLOBAL);
      }


      //Si se activa la interrupción de interfaz con el usuario, se gestiona el proceso
      if(lcd_int)
      {
         //Contadores y valores auxiliares
         int valor, input;
         //Variable para almacenar el nuevo valor del termostato
         float term;

         //Desactivamos interrupciones
         disable_interrupts(GLOBAL);

         //Mostramos un mensaje al usuario para que elija su opción
         lcd_send_byte(LCD_ORDEN,LCD_CLEAR);
         lcd_gotoxy(8,2);
         lcd_putc("1");
         lcd_gotoxy(8,2);
         lcd_send_byte(LCD_ORDEN,LCD_CURSOR); //Activamos el parpadeo del cursor

         input = 1;
         //Mientras que no se pulse SI o NO, esperamos a que el usuario introduzca un número con los cursores
         while(1)
         {
            if(!mas)
            {
               //Si se pulsa el botón de +, se aumenta el valor de input
               input++;
               //Si llegamos al límite de las opciones, reseteamos
               if(input>4)
                  input = 1;
               //Se muestra por pantalla
               printf(lcd_putc,"%i",input);
               //Regresamos el cursor a su posición
               lcd_gotoxy(8,2);
               //Esperamos a que se levante la tecla
               while(!mas){}
               continue;
            }
            if(!menos)
            {
               //Si se pulsa el botón de -, se decrementa el valor de input
               input--;
               //Si llegamos al límite de las opciones, reseteamos
               if(input<1)
                  input = 4;
               //Se muestra por pantalla
               printf(lcd_putc,"%i",input);
               //Regresamos el cursor a su posición
               lcd_gotoxy(8,2);
               //Esperamos a que se levante la tecla
               while(!menos){}
               continue;
            }
            if(!si)
            {
               //Si se pulsa Si, nos quedamos con el valor seleccionado
               //Esperamos a que se suelte la tecla
               while(!si){}
               break;
            }
            if(!no)
            {
               //Si se pulsa No, salimos
               //Esperamos...
               while(!no){}
               //No se muestra mensaje de finalización para ahorrar memoria
               lcd_int = FALSE; //Desactivamos el flag de interrupción
               goto inicio;
            }
         }

         //Se elige la opción pertinente
         switch(input)
         {
            //Si es 1, entramos en la opción de apagado y encendido manual del sistema
            case 1:
                     //Si el sistema está apagado, lo encendemos
                     if(!sistema_encendido)
                     {
                        encender_sistema();
                     }else
                     //Si está encendido, lo apagamos
                     {
                        apagar_sistema();
                        //Si el motor estaba en marcha, debemos apagarlo
                        if(motor)
                           motor = FALSE;

                        //Si estaba encendido por alguna alarma debemos indicarlo
                        if(encendido_por_alarma)
                        {
                           //Recuperamos el valor del termostato
                           termostato=termostato_provisional;
                           //Almacenamos el valor en el sensor
                           ds1820_establecer_TH_TL(127.5, termostato);

                           encendido_por_alarma = FALSE;
                        }
                     }
                     break;


            //Si es 2, entramos en la opción de configuración del termostato
            case 2:

                     //No ponemos encabezado por falta de memoria
                     
                     //Para considerar el valor del termostato hay que tener en cuenta el valor de la histéresis.
                     //Extraemos la parte entera del termostato para así poder trabajar con la función buscar_numero().
                     //Tampoco es una limitación trabajar con valores enteros del termostato, pues nuestro cuerpo no notará
                     //demasiado la diferencia.
                     if(temp_habitacion_superada)
                     {
                        valor = (int)(termostato+histeresis_vivienda);
                        term = termostato+histeresis_vivienda;
                     }
                     else
                     {
                        valor = (int)termostato;
                        term = termostato;
                     }

                     //Pedimos la introducción de nuevo valor
                     lcd_send_byte(LCD_ORDEN, LCD_CLEAR);
                     printf(lcd_putc, "Term:\n       %cC",223);

                     //Recogemos el número de dos cifras introducido por el teclado
                     term=(float)buscar_numero(6, temp_min, temp_max, valor);

                     //Aquí, a diferencia que con el teclado matricial, no es necesario comprobar los límites de temperatura después

                     //Leemos la temperatura
                     temperatura = ds1820_read();
                     if(temperatura >= term)
                     {
                        //Si supera el valor del termostato, lo indicamos con el flag
                        temp_habitacion_superada = TRUE;
                        //Guardamos el termostato nuevo teniendo en cuenta la histeresis de la vivienda
                        termostato = term - histeresis_vivienda;
                     }else
                     {
                        //Si no se supera, se indica también
                        temp_habitacion_superada = FALSE;
                        //Se guarda el valor tal cual
                        termostato = term;
                     }

                     //Escribimos en el sensor el nuevo valor del termostato
                     ds1820_establecer_TH_TL(127.5, termostato);

                     //Guardamos el nuevo valor del termostato en memoria
                     graba_ee(eeprom_termostato, 1, &valor);

                     break;


            //Si es 3, entramos en la opción de configuración de las alarmas
            case 3:

                     //No se muestra encabezamiento por falta de memoria

                     //Se pide la introducción de los valores para la alarma
                     //Se muestra el intervalo a configurar
                     lcd_send_byte(LCD_ORDEN, LCD_CLEAR);
                     printf(lcd_putc, "HORAS\n   :      :");

                     //Se pide al usuario el valor de hora de inicio de la alarma
                     prg.horas_inicio = buscar_numero(2, 0, 23, 0);

                     //Se pide al usuario el valor de los minutos de inicio
                     prg.minutos_inicio = buscar_numero(5, 0, 59, 0);

                     //Al contrario que ocurría en el Proyecto 1 que contaba con teclado matricial,
                     //en este caso comprobamos que la hora final sea posterior a la inicial en el momento.
                     //Para ello debemos establecer el límite inferior de la hora de fin correctamente.
                     
                     valor = prg.horas_inicio;
                     //Si los minutos son 59, tendremos que irnos a la siguiente hora
                     if(prg.minutos_inicio==59)
                        valor += 1;
                        
                     prg.horas_fin = buscar_numero(9, valor, 23, valor);

                     //Si las horas de inicio y fin son las mismas, habrá que limitar que los minutos de finalización
                     //tengan un valor mayor
                     if(prg.horas_inicio == prg.horas_fin)
                        valor = prg.minutos_inicio+1;
                     else
                        valor = 0;
                        
                     prg.minutos_fin = buscar_numero(12, valor, 59, valor); 

                     //El usuario debe ahora introducir el valor del temostato para ese periodo de encendido
                     lcd_send_byte(LCD_ORDEN, LCD_CLEAR);
                     printf(lcd_putc, "Term.\n      %cC", 223);

                     //Recogemos el número introducido por el usuario
                     prg.termostato = buscar_numero(4, temp_min, temp_max, 20); 

                     //Habilitamos la alarma
                     alarma=TRUE;

                     //Programamos la alarma para que salte el reloj en el momento indicado
                     hora_alarma = prg.horas_inicio;
                     minutos_alarma = prg.minutos_inicio;
                     alarma = TRUE;

                     //Indicamos que se trata de una alarma de encendido
                     toca_encender = TRUE;

                     //Guardamos la programación introducida
                     data[0] = prg.horas_inicio;
                     data[1] = prg.minutos_inicio;
                     data[2] = prg.horas_fin;
                     data[3] = prg.minutos_fin;
                     data[4] = prg.termostato;

                     graba_ee(eeprom_programacion, 5, data);

                     break;
                     
            //Si es 4, entramos en el modo de revisión y obtención de datos históricos
            case 4:

                     //En este caso no se pide la introducción de la clave para acortar la longitud del programa
                     //Entramos en el modo revisión

                     //Dejamos la pantalla lcd fija
                     set_tris_d(0xFF);

                     //Representamos el contenido de los registros y activamos la interfaz RS-232
                     representar_registros();

                     break;
         }

         //Se desactiva el flag de indicación de la interrupción
         lcd_int = FALSE;

         //Volvemos a activar las interrupciones
         enable_interrupts(GLOBAL);
      }

      //Comenzamos el bloque de comprobación de parámetros y activación de los procesos necesarios.

      //Comparamos primero la temperatura del agua en la caldera. Esta comprobación se
      //realiza siempre, independientemente de que el sistema esté o no encendido.
      if(!comp_caldera())
      {
         //Entramos aquí si la temperatura de la caldera es menor que la necesaria
         if(temperatura_caldera_superada)
           //Si el flag estaba activado, lo desactivamos
           temperatura_caldera_superada = FALSE;
         if(!caldera_encendida)
            //Si no estaba encendida, la encendemos
            encender_caldera();
      }
      else
      {
         //Si la temperatura de la caldera es mayor que la necesaria, apagamos la caldera si está encendida
         //y activamos el flag correspondiente
         if(!temperatura_caldera_superada)
            temperatura_caldera_superada = TRUE;
         if(caldera_encendida)
            apagar_caldera();
      }

      //Comprobamos la temperatura y almacenamos el resultado en una variable
      aux = comprobar_temperatura();

      //Comprobamos que el sistema no lleve encendido más de 4 horas, ya que en ese caso hay riesgo de avería
      //(en realidad esta limitación deriva de un problema propio de representación en memoria).
      //Ya leimos el tiempo en comprobar_temperatura(), así que no necesitamos leerlo de nuevo.
      if(sistema_encendido && (diferencia_tiempo_sec(hora_encendido, minutos_encendido, 0, tiempo.hours, tiempo.minutes,0))/60>240)
      {
         //Si la diferencia de tiempo es mayor que 4 horas, apagamos el motor y el sistema
         apagar_sistema();

         if(motor)
           motor = FALSE;

         //Si estaba encendido por alguna alarma, recuperamos el termostato anterior.
         if(encendido_por_alarma)
         {
            termostato = termostato_provisional;
            //Almacenamos el valor en el sensor
            ds1820_establecer_TH_TL(127.5, termostato);
            encendido_por_alarma = FALSE;
         }

         goto inicio;

      }

      //Comprobamos ahora la temperatura de la vivienda. Esto se realiza sólo si el sistema está encendido
      if(aux && sistema_encendido && temperatura_caldera_superada)
      {
         //Si la temperatura es menor que la necesaria, el sistema está encendido y la temperatura del
         //agua en la caldera es la adecuada, encendemos el motor de circulación del agua
         if(!motor)
            motor = TRUE;

      }
      else if(motor)
         //Si no se cumplen las condiciones, apagamos el motor si está encendido
         motor = FALSE;

      //Comprobamos si ha saltado la alarma. Esta comprobación la hacemos por software en lugar de por
      //interrupciones porque he tenido muchos problemas con las interrupciones de reloj en la placa
      //que cogí el día de las pruebas, problemas relacionados con el borrado del flag de alarma del
      //reloj. Ante esto, por tanto, aplico la solución gordiana de hacerlo por software para asegurar
      //que funcione en la defensa. Puede verse un ejemplo de aplicación de las interrupciones de reloj
      //en el modelo de PROTEUS del proyecto final.
      //Para detectar si la alarma ha saltado vemos si la diferencia de tiempo entre ambas horas es igual.
      //Esto no nos dará ningún problema en el tiempo de respuesta de la alarma (que se pase al minuto siguiente)
      //ya que el WDT despierta al PIC varias veces por minuto.
      if(alarma && (diferencia_tiempo_sec(hora_alarma, minutos_alarma, 0, tiempo.hours, tiempo.minutes, 0)<59))
      {
         alarma_int=TRUE;
      }

      //Activamos el perro guardián
      setup_wdt(WDT_ON);

      //Ponemos a "dormir" al microcontrolador para que el gasto de energía sea menor
      //Esto lo hacemos sólo si no se han levantado los flags de interrupción en estos segundos,
      //ya que esto provocaría que se aplazase la gestión de la interrupción hasta que saltase el
      //perro guardián.
      if(!alarma_int && !lcd_int)
         sleep();

   }

}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//***************************************************************************
//    Descripción de los métodos utilizados ordenados en orden alfabético
//***************************************************************************
//Puede verse una explicación general de la función de cada método al principio del programa

void apagar_caldera()
{
   int hora, minutos, segundos; //Variables para el tiempo

   //Leemos el tiempo
   PCF8583_read_datetime(&tiempo);

   //Copiamos en las variables
   hora = tiempo.hours;
   minutos = tiempo.minutes;
   segundos = tiempo.seconds;

   //Sumamos al tiempo total de encendido de la caldera el tiempo que ha estado encendida esta vez
   t_total_caldera = t_total_caldera + diferencia_tiempo_sec(hora_caldera, minutos_caldera, segundos_caldera, hora, minutos, segundos);

   //Apagamos la caldera
   caldera_encendida = FALSE;
}

//------------------------------------------------------------------------------------------------------------------------------------

void apagar_sistema()
{
   long minutos_trans;  //Minutos transcurridos desde el encendido del sistema
   int temp; //Temperatura del hogar en ese momento (parte entera y decimal)


   //Si llegamos al límite de la memoria, se resetea el indicador
   //Por utilizar la memoria del PIC sólo contamos con 26 registros de fecha
   if(num_registros > 26)
      num_registros = 0;

   //Leemos el tiempo actual
   PCF8583_read_datetime(&tiempo);

   //Calculamos los minutos transcurridos desde el encendido como una diferencia de tiempos
   minutos_trans = (diferencia_tiempo_sec(hora_encendido, minutos_encendido,0, tiempo.hours, tiempo.minutes,0)/60);

   //Calculamos el valor de la última temperatura cosignada en función de la histéresis
   if(temp_habitacion_superada)
      temp = (unsigned int)(termostato+histeresis_caldera);
   else
      temp = (unsigned int)termostato;

   //Si la caldera estaba encendida, sumamos el último intervalo de encendido al tiempo total
   if(caldera_encendida)
      t_total_caldera = t_total_caldera + diferencia_tiempo_sec(hora_caldera, minutos_caldera, segundos_caldera, tiempo.hours, tiempo.minutes, tiempo.seconds);

   //Calculamos el año en el que nos encontramos
   if(anno_actual_0_to_3 != tiempo.year)
   {
      //Si el año que teníamos almacenado no es igual que el que marca el reloj,
      //debemos de actualizar las variables correspondientes.
      //Suponemos que al menos se conecta el sistema una vez al año
      anno_actual++;
      anno_actual_0_to_3++;

      //Guardamos los valores en memoria
      data[0] = anno_actual;
      data[1] = anno_actual_0_to_3;
      
      graba_ee(eeprom_anno_actual, 2, data);
   }


   //Almacenamos todos los datos requeridos
   data[0] = tiempo.day;
   data[1] = tiempo.month;
   data[2] = anno_actual;
   data[3] = tiempo.hours;
   data[4] = tiempo.minutes;
   data[5] = temp;
   data[6] = (unsigned int)minutos_trans;
   data[7] = (unsigned int)(t_total_caldera/60);
   graba_ee(eeprom_registros + (num_registros)*8 , 8, data);   

   //Aumentamos en uno el número de registros
   num_registros++;

   //Guardamos el número de registros en memoria descomponiendo en dos el número
   data[0] = (int)(num_registros/256);
   data[1] = (int)(num_registros - data[0]*256);

   graba_ee(eeprom_num_registros, 2, data);

   //Finalmente, apagamos el sistema
   sistema_encendido = FALSE;
}

//------------------------------------------------------------------------------------------------------------------------------------

int buscar_numero(int led_unidades, int min, int max, int valor_inicial){
   signed int input;

   //Iniciamos al valor indicado
   input = valor_inicial;

   //Activamos el cursor de la pantalla
   lcd_send_byte(LCD_ORDEN, LCD_CURSOR);

   //Se muestra por pantalla
   lcd_gotoxy(led_unidades,2);
   //Si el número es menor que 10, le añadimos antes un 0
   if(input<10)
      printf(lcd_putc, "0");
   printf(lcd_putc,"%i",input);
   //Regresamos el cursor a su posición
   lcd_gotoxy(led_unidades,2);

   while(1)
   {
      if(!mas)
      {
         //Si se pulsa el botón de +, se aumenta el valor de input
         input++;
         //Si llegamos al límite de los valores, reseteamos
         if(input>max)
            input = min;
         //Si es un número menor que 10, añadimos un 0 antes
         if(input<10)
            printf(lcd_putc,"0");
         //Se muestra por pantalla
         printf(lcd_putc,"%i",input);
         //Regresamos el cursor a su posición
         lcd_gotoxy(led_unidades,2);
         //Esperamos a que se levante la tecla
         while(!mas){}
         continue;
      }
      if(!menos)
      {
         //Si se pulsa el botón de -, se decrementa el valor de input
         input--;
         //Si llegamos al límite de las opciones, reseteamos
         if(input<min)
            input = max;
         //Si es un número menor que 10, añadimos un 0 antes
         if(input<10)
            printf(lcd_putc,"0");
         //Se muestra por pantalla
         printf(lcd_putc,"%i",input);
         //Regresamos el cursor a su posición
         lcd_gotoxy(led_unidades,2);
         //Esperamos a que se levante la tecla
         while(!menos){}
         continue;
      }
      if(!si)
      {
         //Esperamos a que se suelte la tecla
         while(!si){}
         break;
      }      
   }

   //Inhabilitamos el cursor de nuevo
   lcd_send_byte(LCD_ORDEN, LCD_NO_CURSOR);

   //Devolvemos el valor correspondiente
   return input;
}

//------------------------------------------------------------------------------------------------------------------------------------

int buscar_numero_rs232(){
   char c[3]; //Cadena que contendrá los números en caracteres

   //Iniciamos los caracteres a valores nulos que no se correspondan con números
   //Además, esto nos servirá para que c[2] marque el fin de la cadena a la hora de aplicar
   //el método atoi().
   c[0]=c[1]=c[2]='\0';

   //Leemos la primera cifra del número
   while(!isdigit(c[0]))
      c[0]=getch();
   //Imprimimos la primera cifra
   printf("%c", c[0]);

   //Leemos la segunda cifra del número, procediendo de la misma manera
   while(!isdigit(c[1]))
      c[1]=getch();
   printf("%c", c[1]);

   //Transformamos a entero la cadena y la devolvemos
   return atoi(c);

}

//------------------------------------------------------------------------------------------------------------------------------------

int comp_caldera()
{
   //Suponemos una variación de la temperatura del agua en la caldera de t_max_calderaºC
   //a t_min_calderaºC, con una precisión que depende del potenciómetro.

   //Variables para registrar el termostato y la temperatura de la caldera
   float termo, temp;

   //Establecemos como entrada analógica a analizar la del termostato
   set_adc_channel(termostato_caldera);
   delay_us(10);  //Retraso para posibilitar la lectura
   //Transformamos el valor analógico leído en la escala utilizada por el termostato
   termo = t_min_caldera + ((t_max_caldera-t_min_caldera)*(float)read_adc())/AD_num_valores;

   //Establecemos como entrada analógica a analizar la de la temperatura del agua
   set_adc_channel(temperatura_caldera);
   delay_us(10); //Retraso para posibilitar la lectura
   //Transformamos el valor analógico leído a la escala utlizada en la temperatura
   temp = t_min_caldera + ((t_max_caldera-t_min_caldera)*(float)read_adc())/AD_num_valores;

   //Comparamos ambas temperaturas teniendo en cuenta la histéresis de la caldera
   if(temperatura_caldera_superada)
   {
      if(temp >= termo-histeresis_caldera)
         return TRUE;
      else
         return FALSE;
   }else
   {
      if(temp >= termo)
         return TRUE;
      else
         return FALSE;
   }
}

//------------------------------------------------------------------------------------------------------------------------------------

int1 comprobar_temperatura()
{
   //Leemos la temperatura
   temperatura = ds1820_read();

   //Mostramos la información en la pantalla lcd
   mostrar_temperatura();

   if(ds1820_termostato())
   {
      //Si la temperatura es menor que la del termostato, devolvemos TRUE
      if(temp_habitacion_superada)
      {
         //Si la temperatura antes era mayor, indicamos el cambio en el flag
         temp_habitacion_superada = FALSE;
         //Tenemos en cuenta la histéresis para el nuevo valor del termostato
         termostato = termostato + histeresis_vivienda;
         //Guardamos la información también en el sensor de temperatura
         //Ponemos como TH una temperatura muy grande (127.5ºC)
         ds1820_establecer_TH_TL(127.5, termostato);
      }
      return TRUE;
   }
   else
   {
      //Si la temperatura es mayor que la del termostato, devolvemos FALSE
      if(!temp_habitacion_superada)
      {
         //Si antes era menor, cambiamos el flag
         temp_habitacion_superada = TRUE;
         //Guardamos el nuevo valor del termostato teniendo en cuenta la histéresis
         termostato = termostato - histeresis_vivienda;
         //Guardamos la información también en el sensor de temperatura
         //Ponemos como TH una temperatura muy grande (127.5ºC)
         ds1820_establecer_TH_TL(127.5, termostato);
      }
      return FALSE;
   }
}

//------------------------------------------------------------------------------------------------------------------------------------

long diferencia_tiempo_sec(int hora1, int min1, int sec1, int hora2, int min2, int sec2)
{
   unsigned long dif;
   unsigned long dif2;
   
   //Si la hora y los minutos son iguales, devolvemos simplemente la diferencia en segundos
   if(hora1==hora2 && min1==min2)
      dif = abs(sec2-sec1);
   else
   {
      //Si no habrá que tener en cuenta también la diferencia entre horas y minutos
      dif2 = ((long)hora2*60 + (long)min2) - ((long)hora1*60 + (long)min1);
      //No hacemos la diferencia en segundos directamente porque el long no da de sí
      dif = dif2*60 + sec2 - sec1;
   }   

   //Devolvemos el resultado
   return dif;
}

//------------------------------------------------------------------------------------------------------------------------------------

void encender_caldera()
{
   //Leemos el tiempo actual
   PCF8583_read_datetime(&tiempo);

   //Guardamos los datos en las variables correspondientes
   hora_caldera = tiempo.hours;
   minutos_caldera = tiempo.minutes;
   segundos_caldera = tiempo.seconds;

   //Encendemos la caldera
   caldera_encendida = TRUE;
}

//------------------------------------------------------------------------------------------------------------------------------------

void encender_sistema()
{
   //Leemos el tiempo actual
   PCF8583_read_datetime(&tiempo);

   //Registramos la hora y minutos en los que se enciende el sistema
   hora_encendido    = tiempo.hours;
   minutos_encendido = tiempo.minutes;

   //Inicializamos a 0 el tiempo total que está encendida la caldera
   t_total_caldera = 0;

   //Si la caldera está encendida, inicializamos el instante de encendido a
   //éste.
   if(caldera_encendida)
   {
      hora_caldera = tiempo.hours;
      minutos_caldera = tiempo.minutes;
      segundos_caldera = tiempo.seconds;
   }

   //Encendemos el sistema
   sistema_encendido = TRUE;
}

//------------------------------------------------------------------------------------------------------------------------------------

void inicializacion()
{
   //NOTA: todos los pequeños retardos introducidos en esta rutina sirven para que el técnico
   //no se sienta atosigado por los continuos mensajes que le aparecen solicitándole información.


   //Variables para introducir los datos de fecha y hora
   int weekday, dia, mes, anno, horas, minutos;

   char c[2]; //Cadena para la conversión a int

   //Comenzamos pidiendo la introducción de la fecha
   //Es una lástima que en RS232 no se vean bien las tildes
   e1:
   printf("\rInicializacion\r");
   printf("\r");
   //Primero solicitamos el día de la semana
   printf("LUN");

   //En principio se indica que es lunes
   weekday = 0;

   //Inicializamos el vector de caracteres a \0 por dos motivos según la posición:
   //c[0] para que no se elija un día indeseado sin intervención del técnico y c[1],
   //para marcar el fin de la cadena.
   c[0]=c[1]='\0';


   //Iteramos en el bucle hasta que no se pulse ENTER
   while(c[0]!=13){
      //Guardamos el caracter leído
      c[0]=getch();

      //Además se asigna un valor numérico a la variable weekday para almacenarla en el reloj.
      weekday=atoi(c)-1;

      //Según el día introducido, imprimimos un resultado u otro por pantalla, borrando el
      //anterior.
      switch(weekday)
      {
         case 0: printf("\b\b\bLUN");
                 break;
         case 1: printf("\b\b\bMAR");
                 break;
         case 2: printf("\b\b\bMIE");
                 break;
         case 3: printf("\b\b\bJUE");
                 break;
         case 4: printf("\b\b\bVIE");
                 break;
         case 5: printf("\b\b\bSAB");
                 break;
         case 6: printf("\b\b\bDOM");
      }
   }

   //Pedimos la introducción de la fecha
   printf(" ");

   //Leemos el día del mes
   dia = buscar_numero_rs232();
   //Si pulsamos "backspace" o el día es incorrecto, se resetea la inicialización
   if(dia>31 || dia==0)
   {
      goto e1;
   }

   //Leemos el mes
   printf("/");
   mes = buscar_numero_rs232();
   //Si pulsamos "backspace", el mes es incorrecto o el día no se encuentra en el mes seleccionado,
   //se resetea la inicialización
   if(mes>12 || mes==0 || (dia>29 && mes==2) || (dia==31 && (mes==4 || mes==6 || mes==9 || mes==11)))
   {
      goto e1;
   }

   //Leemos el año
   printf("/");
   anno = buscar_numero_rs232();
   //Si pulsamos "backspace" o el valor del año es incorrecto, se resetea la inicialización
   //Tampoco se permite un año menor que 2009 (año de fabricación del sistema de control).
   //También salta si el año no es bisiesto y hemos seleccionado el 29 de febrero.
   if(anno>99 || anno<9 || (anno%4!=0 && mes==2 && dia==29))
   {
      goto e1;
   }

   //Calculamos el año en el intervalo de 0 a 3 según sean bisiesto este año
   //o los anteriores.
   if(anno%4==0)
      anno_actual_0_to_3 = 0;
   else if((anno-1)%4==0)
      anno_actual_0_to_3 = 1;
   else if((anno-2)%4==0)
      anno_actual_0_to_3 = 2;
   else if((anno-3)%4==0)
      anno_actual_0_to_3 = 3;

   //Se pide la introducción de la hora
   printf(" ");

   //Leemos la hora
   horas = buscar_numero_rs232();
   //Si se pulsa "backspace" o la hora es incorrecta, se resetea todo el proceso
   if(horas>23)
   {
      goto e1;
   }

   //Leemos los minutos
   printf(":");
   minutos = buscar_numero_rs232();

   //Si se pulsa "backspace" o los minutos son incorrectos, reseteamos la inicialización completa
   if(minutos>59)
   {
      goto e1;
   }

   //Mostramos un mensaje de guardado de configuración
   printf("\rGuardando...");

   //Almacenamos todo en la estructura que contiene la información del tiempo
   tiempo.month   = mes;
   tiempo.day     = dia;
   tiempo.year    = anno_actual_0_to_3;
   tiempo.hours   = horas;
   tiempo.minutes = minutos;
   tiempo.weekday = weekday;

   //Fijamos el reloj a esa fecha y hora
   PCF8583_set_datetime(&tiempo);

   //Establecemos los parámetros iniciales del sistema para resetear la memoria
   termostato = (float)20;
   num_registros = 0;
   anno_actual = anno;

   //Escribimos los datos en la EEPROM
   data[0] = 20;
   data[1] = 0;
   data[2] = 0;
   data[3] = anno;
   data[4] = anno_actual_0_to_3;
   graba_ee(eeprom_termostato, 5, data);   

   //Escribimos el valor del termostato en el sensor de temperatura.
   //Ponemos como valor alto 127.5 grados, temperatura que no se va a alcanzar.
   ds1820_establecer_TH_TL(127.5, termostato);
}

//------------------------------------------------------------------------------------------------------------------------------------

void mostrar_temperatura()
{
   //Leemos el tiempo y lo guardamos en la variable tiempo
   PCF8583_read_datetime(&tiempo);
   //Activamos las interrupciones, que se desactivan dentro del método
   enable_interrupts(GLOBAL);

   //Mostramos la temperatura registrada
   lcd_send_byte(LCD_ORDEN, LCD_CLEAR);
   printf(lcd_putc, "TEMP-%3.1f %cC\nHORA-%u:%u", temperatura, 223,tiempo.hours, tiempo.minutes);
}

//------------------------------------------------------------------------------------------------------------------------------------

void representar_registros()
{
   //Contadores para los bucles
   unsigned int contador;
   //Caracter para la introducción de las opciones
   char caracter;

   float media_dia_caldera;         //Tiempo medio que se enciende la caldera cada día
   float valor;                     //Tiempo medio en minutos que se enciende el sistema por día

   unsigned long valor_total;  //Tiempo total de encendido de la caldera
   unsigned long valor_total_caldera; //Tiempo total de encendido del sistema

   //Variables para almacenar valores provisionales
   long tiempo_x_dia;         //Tiempo que se enciende cada día el sistema (minutos)
   long tiempo_x_dia_caldera; //Tiempo que se enciende cada día la caldera (minutos)
   int fecha[3];             //Día con el que estamos trabajando (día/mes/año)
   long num_dias;             //Número de días que llevamos analizados

   //Iniciamos a cero las otras variables
   fecha[0] = fecha[1] = fecha[2] = 0;
   num_dias = 0;

   media_dia_caldera = 0;
   valor = 0;
   valor_total = 0;
   valor_total_caldera = 0;

   //Imprimimos el encabezamiento
   printf("\rREGISTRO DE EVENTOS\r\r");
   printf("Fecha           Temp.    Min.S.  Min.C.\r");
   printf("=======================================\r");

   //Recorremos todos los registros almacenados
   for(contador=0; contador<num_registros; contador++)
   {
      //Comenzamos leyendo los datos de memoria
      lee_ee(eeprom_registros + contador*8, 8, data);

      //Si cambiamos de día debemos gestionar los datos del nuevo día y almacenar los del anterior
      if(fecha[0]!=data[0] || fecha[1]!=data[1] || fecha[2]!=data[2])
      {
         //Lógicamente, no tenemos en cuenta para la media los días en los que no se enciende:
         //días de verano, calurosos, etc. Esta información puede visualizarse en la media
         //por meses.
         //Guardamos los datos del día anterior si el día no es el primero
         if(fecha[0]!=0)
         {
            //En la media de tiempo por día, acumulamos el tiempo que ha estado encendido el último día
            valor = valor + (float)tiempo_x_dia;
            //Realizamos la misma operación con la media de encendido de la caldera
            media_dia_caldera = media_dia_caldera + (float)tiempo_x_dia_caldera;
         }
         //La sumatoria podría alcanzar un valor muy alto. El sistema no está preparado para estas eventualidades
         //y debería ser reseteado cada cierto tiempo. Si el sistema va a utilizarse mucho (zonas muy frías o mal
         //aisladas), el usuario debería informar al fabricante para aumentar la memoria de almacenamiento y el
         //tamaño de las variables

         //Modificamos la información del día
         fecha[0] = data[0];
         fecha[1] = data[1];
         fecha[2] = data[2];

         //Aumentamos en 1 el número de días
         num_dias++;

         //Reseteamos las variables acumulativas de tiempo por día
         tiempo_x_dia = 0;
         tiempo_x_dia_caldera = 0;
      }

      //Le sumamos al tiempo de encendido de sistema y caldera el correspondiente de este registro
      tiempo_x_dia = tiempo_x_dia + data[6];
      tiempo_x_dia_caldera = tiempo_x_dia_caldera + data[7];
      //También incrementamos los tiempos totales
      valor_total = valor_total + data[6];
      valor_total_caldera = valor_total_caldera + data[7];

      //Representamos todos los datos, teniendo en cuenta algunos requisitos representativos
      //para que los datos queden alineados (número menores que 10, que 100, etc).
      if(data[0]<10)
         printf(" ");

      printf("%u/", data[0]);

      if(data[1]<10)
         printf("0");

      printf("%u/", data[1]);

      if(data[2]<10)
         printf("0");

      printf("%u  ", data[2]);

      if(data[3]<10)
         printf("0");

      printf("%u:", data[3]);

      if(data[4]<10)
         printf("0");

      printf("%u ", data[4]);

      if(data[5]<10)
         printf(" ");

      printf("%u      ", data[5]);      

      if(data[6]<100)
         printf(" ");
      if(data[6]<10)
         printf(" ");

      printf("%u     ", data[6]);

      if(data[7]<100)
         printf(" ");
      if(data[7]<10)
         printf(" ");

      printf("%u\r", data[7]);

   }

   //Debemos realizar la operación de guardado para el último día. El proceso es el mismo
   //que hemos seguido arriba.
   if(fecha[0]!=0)
   {
      valor = valor + (float)tiempo_x_dia;
      media_dia_caldera = media_dia_caldera + (float)tiempo_x_dia_caldera;
   }

   //Calculamos la media como el cociente entre los valore de tiempo calculados y el número de
   //días en los que se ha encendido la caldera
   valor = valor/(float)num_dias;
   media_dia_caldera = media_dia_caldera/(float)num_dias;

   repetir:
   //Menú para mostrar los datos
   printf("\r");
   printf("0.Salir\r");
   printf("1.Tiempos medios por dia\r");
   printf("2.Tiempos totales\r");
   printf("\r");

   //Esperamos la introducción de una de las opciones por parte del técnico.
   //Cuando se pulse uno de los botones, asignamos a la variable caracter el valor correspondiente.
   caracter='J';
   while(caracter!='0' && caracter!='1' && caracter!='2'){
      caracter=getch();
   }

   switch(caracter)
   {
      //En el caso 0, se sale del sistema
      case '0':
               //No se indica la salida para ahorrar memoria
               break;

      //En el caso 1, se muestran los valores medios
      case '1':
               printf("\rT. med.: %3.2f min\r\r", valor);
               printf("T. med. caldera: %3.2f min\r\r", media_dia_caldera);
               goto repetir;
               break;

      //En el caso 2, se muestran los valores totales
      case '2':
               printf("\rT. total: %lu min\r\r", valor_total);
               printf("T. total caldera: %lu min\r\r", valor_total_caldera);
               goto repetir;
               break;

      //En los dos casos, vuelve a mostrarse el menú tras imprimir las estadísticas
   }
}

//------------------------------------------------------------------------------------------------------------------------------------
