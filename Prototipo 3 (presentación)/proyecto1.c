//*****************************************************************
//***   PROYECTO 1: Control de un sistema de calefacción de una ***
//***   vivienda utilizando un microcontrolador PIC18F4520      ***
//*****************************************************************
//Author: Julio Navarro Lara        2010

//En este caso está preparado para probarse en la placa EasyPic3



#include "18F4520.H"
#include "proyecto1.h"
#fuses HS, NOPROTECT, BROWNOUT, PUT, NOLVP, NOXINST, WDT4096, NOWDT
//El watchdog-timer salta cada 16.384 segundos, aproximadamente
#use delay(clock=8000000, restart_wdt)
#use rs232(baud=4800, xmit=PIN_C6,rcv=PIN_C7)
#include <ctype.h>
#include <stdlib.h>
#include "PCF8583.c"
#include "LCDeasy.c"
#include "1wire.c"
#include "ds1820.c"


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

//Vector que incluye todas las programaciones horarias
//La información se distribuye en grupos de 5: hora_inicio, minutos_inicio,
//hora_fin, minutos_fin y termostato.
programacion programaciones[3];

//Programación en curso en este momento
programacion prg;
int num_intervalos; //Número de intervalos de programación con los que contamos
int posicion_alarmas; //Variable que apunta a la posición de la próxima alarma

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
unsigned int num_registros;  //Número de registros de fechas almacenados
int anno_actual;             //Año en el que estamos (de 0 a 99)
int anno_actual_0_to_3;      //Año en el que estamos (de 0 a 3, a pesar de
                              //que la variable se llame 1_to_3 por razones "históricas")

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
//se muestra la variable al principio. Si se pulsa SI, se aprueba el valor que aparece, y si aparece NO, se
//devuelve el código correspondiente.
int buscar_numero(int led_unidades, int min, int max, int valor_inicial);
//buscar_numero_rs232() -> Registra la introducción de un número de dos cifras mediante la interfaz RS232 y
//lo devuelve. Además, si se pulsa el "backspace" durante el proceso, devuelve el código correspondiente a NO
int buscar_numero_rs232();
//comp_caldera() -> Devuelve TRUE si debemos apagar la caldera. Esta función tiene en cuenta
//cierta histéresis para evitar ciclos continuos de apagado-encendido.
int comp_caldera();
//comprobar_hora(int hora1, int min1, int hora2, int min2) -> Devuelve TRUE
//si hora1:min1 corresponde a un tiempo menor o igual que hora2:min2.
int comprobar_hora(int hora1,int min1, int hora2, int min2);
//comprobar_temperatura() -> Devuelve TRUE si la temperatura de la habitación es menor
//que la indicada en el termostato, y FALSE en caso contrario.
int comprobar_temperatura();
//diferencia_tiempo(int hora1, int min1, int hora2, int min2) -> Calcula la
//diferencia de tiempo en minutos entre hora1:min1 y hora2:min2.
//Si hora1:min1 es mayor que hora2:min2, los considera en días consecutivos.
long diferencia_tiempo(int hora1,int min1, int hora2, int min2);
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
//grabar_programaciones() -> Graba los datos de las programaciones en la memoria EEPROM.
void grabar_programaciones();
//inicializacion() -> Gestiona la inicialización del sistema por parte del técnico.
void inicializacion();
//leer_programaciones() -> Recupera de la memoria los datos de las programaciones.
void leer_programaciones();
//mostrar_temperatura() -> Muestra en la pantalla LCD la temperatura actual y la hora.
void mostrar_temperatura();
//ordenar_programaciones -> Ordena las programaciones por orden creciente de hora.
void ordenar_programaciones();
//programar_proxima_alarma() -> Programa la próxima alarma que tendrá lugar.
//Siempre se tratará de una alarma para el encendido.
void programar_proxima_alarma();
//representar_registros() -> Representa la información de los registros en el puerto RS232
//Nunca guardamos la información de los registros para no saturar la memoria y
//aprovechamos la representación de los datos para calcular medias y demás y así
//no tener que realizar un segundo barrido de lectura en memoria.
void representar_registros();
//seleccionar_alarma() -> Selecciona la alarma más cercana a la hora en la que nos encontramos,
//en el caso de que las hubiese.
void seleccionar_alarma();

//--------------------------------------------------------------------------------

//Gestión de interrupciones de activación de la interfaz de usuario
#INT_EXT1
void activacion_led(){
   clear_interrupt(INT_EXT1);
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

   //Determinamos que las patilla de interrupción se activen al pasar de alta a baja
   ext_int_edge(1, H_TO_L);

   clear_interrupt(INT_EXT1);

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

   //Mostramos un mensaje de inicio
   lcd_send_byte(LCD_ORDEN, LCD_CLEAR);
   lcd_send_byte(LCD_ORDEN, LCD_NO_CURSOR);
   printf(lcd_putc, "Inicializando...");

   //-----------------------------------------------------------
   //Rutina de inicialización del sistema por parte del técnico
   //-----------------------------------------------------------
   inicializacion();

   //Propiedades de conversión analógico digital
   //Caracterizamos los puertos A0 a A3 como entradas analógicas
   setup_adc_ports(AN0_TO_AN3);
   //Definimos el reloj de conversión
   setup_adc(adc_clock_div_32);

   /*
   //El siguiente fragmento de código se descomentaría en el caso de que quiera obviarse la fase
   //de inicialización del sistema y quieran leerse datos conservados en memoria, si, por ejemplo,
   //ya tenemos el reloj configurado. En ese caso, debemos borrar la sentencia de "inicializacion" de arriba.
   //********************************************************************************

   //Recuperación de datos de la memoria
   //Recuperamos a partir de la dirección eeprom_termostato los datos en
   //el siguiente orden: termostato, número de intervalos de alarma, número de registros
   //de encendido de la caldera, año actual en valores de 0 a 99 y año actual en valores de 0 a 3.
   termostato = read_eeprom(eeprom_termostato);
   num_intervalos = read_eeprom(eeprom_num_intervalos);
   num_registros = read_eeprom(eeprom_num_registros);
   anno_actual = read_eeprom(eeprom_anno_actual);
   anno_actual_0_to_3 = read_eeprom(eeprom_anno_0_to_3);


   //Lee las programaciones de alarmas guardadas en memoria
   leer_programaciones();


   //Comprobamos si hay alarmas programadas en memoria
   if(num_intervalos!=0)
   {
      //Si las hay, seleccionamos la siguiente y programamos su activación
      seleccionar_alarma();
      programar_proxima_alarma();
      toca_encender = TRUE;
   }

   //Iniciamos la cuenta del reloj
   PCF8583_write_byte(PCF8583_CTRL_STATUS_REG,
                           PCF8583_START_COUNTING);

   */


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
   enable_interrupts(INT_EXT1);
   enable_interrupts(GLOBAL);

   //Mostramos un mensaje que indica al técnico el éxito en la inicialización
   printf("Sistema inicializado con exito!\r");

   //Repetimos la siguiente rutina hasta que se desconecte el micro
   while(1){
      inicio:
      //Inhabilitamos el perro guardián para que no interfiera con las gestión de interrupciones y
      //con la medida de la temperatura
      setup_wdt(WDT_OFF);

      //Borramos los flags de interrupción que pudiesen estar activos
      clear_interrupt(INT_EXT1);

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
               //Mostramos un mensaje
               lcd_send_byte(LCD_ORDEN,LCD_CLEAR);
               printf(lcd_putc, "  Encendiendo\n  sistema...");
               //Encendemos el sistema
               encender_sistema();

               //Recogemos el tiempo en el que debe de saltar la alarma de apagado.
               
               hora_alarma = prg.horas_fin;
               minutos_alarma = prg.minutos_fin;


            }else
            {
               //Si se trata de una alarma de encendido y el sistema está encendido
               delay_ms(250); //Retraso para evitar solapamiento de alarmas

               //Nos vamos a la siguiente alarma
               posicion_alarmas++;
               //Si hemos llegado al final de la lista, volvemos al principio
               if(posicion_alarmas==num_intervalos)
                  posicion_alarmas = 0;

               //Programamos la próxima alarma de encendido
               programar_proxima_alarma();

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
               //Mostramos un mensaje
               lcd_send_byte(LCD_ORDEN,LCD_CLEAR);
               printf(lcd_putc, "  Apagando\n  sistema...");
               //Apagamos el sistema
               apagar_sistema();
            }

            //Nos vamos a la siguiente alarma
            posicion_alarmas++;
            //Si llegamos al final de la lista, volvemos al principio
            if(posicion_alarmas==num_intervalos)
               posicion_alarmas = 0;

            //Programamos la próxima alarma de encendido
            programar_proxima_alarma();

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
         int contador, contador2, valor, input;
         //Variable para almacenar el nuevo valor del termostato
         float term;
         //Variables provisionales para buscar en la lista de programaciones
         programacion pr, pr2;

         //Desactivamos interrupciones
         disable_interrupts(GLOBAL);

         //Mostramos un mensaje al usuario para que elija su opción
         lcd_send_byte(LCD_ORDEN,LCD_CLEAR);
         printf(lcd_putc, "  Indique la \n  operacion   1");
         lcd_gotoxy(15,2);
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
               lcd_gotoxy(15,2);
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
               lcd_gotoxy(15,2);
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
               //Se muestra un mensaje de finalización
               lcd_send_byte(LCD_ORDEN,LCD_CLEAR);
               lcd_send_byte(LCD_ORDEN,LCD_NO_CURSOR);
               printf(lcd_putc, "  Anulando\n  operacion...");
               delay_ms(LCD_T_RETARDO);
               lcd_int = FALSE; //Desactivamos el flag de interrupción
               goto inicio;
            }
         }

         lcd_send_byte(LCD_ORDEN, LCD_NO_CURSOR);

         //Se elige la opción pertinente
         switch(input)
         {
            //Si es 1, entramos en la opción de apagado y encendido manual del sistema
            case 1:
                     //Si el sistema está apagado, lo encendemos
                     if(!sistema_encendido)
                     {
                        lcd_send_byte(LCD_ORDEN,LCD_CLEAR);
                        printf(lcd_putc, "  Encendiendo\n  sistema...");
                        encender_sistema();
                     }else
                     //Si está encendido, lo apagamos
                     {
                        lcd_send_byte(LCD_ORDEN,LCD_CLEAR);
                        printf(lcd_putc, "  Apagando\n  sistema...");
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

                     //Mostramos el valor del termostato actual
                     lcd_send_byte(LCD_ORDEN, LCD_CLEAR);
                     //Para considerar el valor del termostato hay que tener en cuenta el valor de la histéresis.
                     //Extraemos la parte entera del termostato para así poder trabajar con la función buscar_numero().
                     //Tampoco es una limitación trabajar con valores enteros del termostato, pues nuestro cuerpo no notará
                     //demasiado la diferencia.
                     if(temp_habitacion_superada)
                     {
                        valor = (int)(termostato+histeresis_vivienda);
                        printf(lcd_putc, "Modificar\ntermostato %.0f%cC",termostato+histeresis_vivienda,223);
                     }
                     else
                     {
                        valor = (int)termostato;
                        printf(lcd_putc, "Modificar\ntermostato %.0f%cC",termostato,223);
                     }

                     //Retardamos cierto tiempo para que el usuario vea correctamente los datos de la pantalla
                     delay_ms(LCD_T_RETARDO*2);

                     //Pedimos la introducción de nuevo valor
                     lcd_send_byte(LCD_ORDEN, LCD_CLEAR);
                     printf(lcd_putc, "Nuevo valor:\n       %cC",223);

                     //Recogemos el número de dos cifras introducido por el teclado
                     term=(float)buscar_numero(6, temp_min, temp_max, valor);

                     //Si se pulsa la tecla SI o NO, salimos
                     if(term==NOCODE)
                        goto salir;
                     else
                     {
                        //Retardamos cierto tiempo para permitir al usuario visualizar el valor que ha introducido
                        delay_ms(LCD_T_RETARDO);

                        //Mostramos un mensaje de aplicación de cambios
                        lcd_send_byte(LCD_ORDEN, LCD_CLEAR);
                        printf(lcd_putc, "  Aplicando\n  cambios...");

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
                        write_eeprom(eeprom_termostato, (int)term);


                     }

                     break;


            //Si es 3, entramos en la opción de configuración de las alarmas
            case 3:

                     //Mostramos el encabezamiento de la operación
                     lcd_send_byte(LCD_ORDEN, LCD_CLEAR);
                     printf(lcd_putc," Programar\n sistema");
                     delay_ms(LCD_T_RETARDO);

                     //Tenemos la posibilidad de introducir 3 intervalos de alarma.
                     //Se va guiando al usuario por la configuración de cada intervalo mediante el siguiente bucle
                     for(contador=0; contador<3; contador++)
                     {
                        //Se muestra el intervalo a configurar
                        lcd_send_byte(LCD_ORDEN, LCD_CLEAR);
                        printf(lcd_putc, " INTERVALO %i\n   :      :",contador+1);

                        //Se pide al usuario el valor de hora de inicio de la alarma
                        pr.horas_inicio = buscar_numero(2, 0, 23, 0);

                        //Si se pulsa No, se indica que no quieren introducirse más alarmas.
                        //En el otro código esto se gestionaba pulsando sí y con no simplemente se salía,
                        //pero aquí debemos hacerlo así por falta de botones...
                        if(pr.horas_inicio==NOCODE)
                        {
                           //Si era la primera alarma que se gestionaba, se interpreta como que
                           //no se quiere activar ninguna alarma
                           if(contador==0)
                           {
                              //Desactivamos las alarmas
                              alarma = FALSE;
                              //Salimos
                              goto salir;
                           }
                           else
                              //Salimos simplemente del bucle
                              break;
                        }

                        //Se pide al usuario el valor de los minutos de inicio
                        pr.minutos_inicio = buscar_numero(5, 0, 59, 0);

                        //Si se pulsa NO, interrumpimos la introducción de alarmas
                        if(pr.minutos_inicio==NOCODE)
                        {
                           //Si es el primer intervalo, interpretamos como que el usuario
                           //no quiere programar alarmas
                           if(contador==0)
                           {
                              //Desactivamos las alarmas
                              alarma = FALSE;
                              goto salir;
                           }
                           else
                              //Salimos del bucle
                              break;
                        }

                        //Al contrario que ocurría en el Proyecto 1 que contaba con teclado matricial,
                        //en este caso comprobamos que la hora final sea posterior a la inicial en el momento.
                        //Para ello debemos establecer el límite inferior de la hora de fin correctamente.

                        //Si la hora de inicio corresponde a la última hora del día, no podrá programarse nada
                        if(pr.horas_inicio==23 && pr.minutos_inicio==59)
                        {
                           //Si se produce un error de este tipo, se indica
                           lcd_send_byte(LCD_ORDEN, LCD_CLEAR);
                           printf(lcd_putc, "   Valores\n   incorrectos");
                           delay_ms(LCD_T_RETARDO);
                           //Se repite la última iteración del bloque
                           contador--;
                           continue;
                        }

                        //Si los minutos son 59, tendremos que irnos a la siguiente hora
                        if(pr.minutos_inicio==59)
                        {
                           pr.horas_fin = buscar_numero(9, pr.horas_inicio+1, 23, pr.horas_inicio+1);
                        }else
                        {
                           pr.horas_fin = buscar_numero(9, pr.horas_inicio, 23, pr.horas_inicio);
                        }

                        if(pr.horas_fin==NOCODE)
                        {
                           if(contador==0)
                           {
                              alarma = FALSE;
                              goto salir;
                           }
                           else
                              break;
                        }

                        //Si las horas de inicio y fin son las mismas, habrá que limitar que los minutos de finalización
                        //tengan un valor mayor
                        if(pr.horas_inicio == pr.horas_fin)
                           pr.minutos_fin = buscar_numero(12, pr.minutos_inicio+1, 59, pr.minutos_inicio+1);
                        else
                           pr.minutos_fin = buscar_numero(12, 0, 59, 0);

                        if(pr.minutos_fin==NOCODE)
                        {
                           if(contador==0)
                           {
                              alarma = FALSE;
                              goto salir;
                           }
                           else
                              break;
                        }

                        //Introducimos cierto retardo para que el usuario pueda visualizar los valores introducidos en la pantalla
                        delay_ms(LCD_T_RETARDO);

                        //No puede haber un intervalo de encendido de más de 4 horas.
                        //Esto deriva de un problema propio de almacenamiento de números enteros, pero es perfectamente lógico
                        //que un sistema de calefacción tenga un tiempo máximo de encendido para regular el uso abusivo del sistema
                        //y evitar averías por un funcionamiento muy prolongado.
                        if(diferencia_tiempo(pr.horas_inicio, pr.minutos_inicio, pr.horas_fin, pr.minutos_fin)>240)
                        {
                           //Mostramos un aviso
                           lcd_send_byte(LCD_ORDEN, LCD_CLEAR);
                           printf(lcd_putc, "Intervalo limite\nde 4 horas");
                           delay_ms(LCD_T_RETARDO);
                           //Se repite la última iteración del bloque
                           contador--;
                           continue;

                        }

                        //El usuario debe ahora introducir el valor del temostato para ese periodo de encendido
                        lcd_send_byte(LCD_ORDEN, LCD_CLEAR);
                        printf(lcd_putc, "Termostato %i\n      %cC", contador+1, 223);

                        //Recogemos el número introducido por el usuario
                        pr.termostato = buscar_numero(4, temp_min, temp_max, 20);

                        //Retardamos para que el usuario pueda ver el valor que ha introducido
                        delay_ms(LCD_T_RETARDO);

                        //Comprobamos que los intervalos sean correctos respecto a los otros, es decir,
                        //que no haya solapamiento entre los distintos intervalos programados.
                        for(contador2=0; contador2<contador; contador2++)
                        {
                           //Tomamos las programaciones almacenadas anteriormente
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
                              //Si hay solapamiento, mostramos un mensaje de error y salimos
                              lcd_send_byte(LCD_ORDEN, LCD_CLEAR);
                              printf(lcd_putc, "Solapamiento\nde intervalos!");
                              delay_ms(LCD_T_RETARDO*2);
                              goto salir;
                           }
                        }

                        //Añadimos esta programación a la lista de programaciones
                        programaciones[contador]=pr;
                     }

                     //Habilitamos la alarma
                     alarma=TRUE;

                     //Guardamos el número de intervalos configurados
                     num_intervalos = contador;

                     //Ordenamos las programaciones por orden de hora de inicio
                     ordenar_programaciones();

                     //Seleccionamos la próxima alarma a ejecutar
                     seleccionar_alarma();

                     //Programamos la próxima alarma para que salte el reloj en el momento indicado
                     programar_proxima_alarma();

                     //Indicamos que se trata de una alarma de encendido
                     toca_encender = TRUE;

                     //Guardamos las programaciones introducidas
                     grabar_programaciones();

                     break;


            //Si es 4, entramos en el modo de revisión y obtención de datos históricos
            case 4:

                     //Solicitamos la introducción de una clave.
                     //Al contrario de lo que ocurría cuando disponíamos de teclado matricial,
                     //en este caso la clave es una combinación de botones: {+,-,-,SI}
                     lcd_send_byte(LCD_ORDEN, LCD_CLEAR);
                     printf(lcd_putc, "  Introduzca\n  clave");

                     //Realizamos todo el proceso de comprobación de clave correcta según los botones pulsados
                     while(1)
                     {
                        if(!mas)
                        {
                           while(!mas){}
                           while(1)
                           {
                              if(!menos)
                              {
                                 while(!menos){}
                                 while(1)
                                 {
                                    if(!menos)
                                    {
                                       while(!menos){}
                                       while(1)
                                       {
                                          if(!si)
                                          {
                                             while(!si){}
                                             goto clave_correcta;
                                          }
                                          if(!mas || !menos || !no)
                                          {
                                             while(!mas || !menos || !no){}
                                             goto clave_incorrecta;
                                          }
                                       }
                                    }
                                    if(!mas || !si || !no)
                                    {
                                       while(!mas || !si || !no){}
                                       goto clave_incorrecta;
                                    }
                                 }
                              }
                              if(!mas || !si || !no)
                              {
                                 while(!mas || !si || !no){}
                                 goto clave_incorrecta;
                              }
                           }
                        }
                        if(!menos || !si || !no)
                        {
                           while(!menos || !si || !no){}
                           goto clave_incorrecta;
                        }
                     }

                     clave_incorrecta:

                     //Si la clave es incorrecta, lo indicamos y salimos
                     lcd_send_byte(LCD_ORDEN, LCD_CLEAR);
                     printf(lcd_putc, "  Clave\n  incorrecta");
                     delay_ms(LCD_T_RETARDO);
                     goto salir;

                     clave_correcta:

                     //Si la clave es correcta, entramos en el modo revisión
                     lcd_send_byte(LCD_ORDEN, LCD_CLEAR);
                     printf(lcd_putc, "  Modo\n  revision");

                     //Dejamos la pantalla lcd fija
                     set_tris_d(0xFF);

                     //Representamos el contenido de los registros y activamos la interfaz RS-232
                     representar_registros();


                     break;

              default:

                        //Aquí confluyen todas las salidas de la rutina de interfaz con el usuario
                        salir:
                        //Se muestra un mensaje de finalización
                        lcd_send_byte(LCD_ORDEN,LCD_CLEAR);
                        lcd_send_byte(LCD_ORDEN,LCD_NO_CURSOR);
                        printf(lcd_putc, "  Anulando\n  operacion...");
                        delay_ms(LCD_T_RETARDO);

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
      if(sistema_encendido && diferencia_tiempo(hora_encendido, minutos_encendido, tiempo.hours, tiempo.minutes)>240)
      {
         //Si la diferencia de tiempo es mayor que 4 horas, lo indicamos y apagamos el motor y el sistema
         lcd_send_byte(LCD_ORDEN,LCD_CLEAR);
         printf(lcd_putc, "Demasiado tiempo\nencendido!");
         delay_ms(2*LCD_T_RETARDO);
         lcd_send_byte(LCD_ORDEN,LCD_CLEAR);
         printf(lcd_putc, "  Apagando\n  sistema...");
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
      if(alarma && (diferencia_tiempo(hora_alarma, minutos_alarma, tiempo.hours, tiempo.minutes)==0))
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
   int t_entera, t_decimal; //Temperatura del hogar en ese momento (parte entera y decimal)
   float temperatura;  //Temperatura leída


   //Si llegamos al límite de la memoria, se resetea el indicador
   //Por utilizar la memoria del PIC sólo contamos con 26 registros de fecha
   if(num_registros > 26)
      num_registros = 0;

   //Leemos el tiempo actual
   PCF8583_read_datetime(&tiempo);

   //Calculamos los minutos transcurridos desde el encendido como una diferencia de tiempos
   minutos_trans = diferencia_tiempo(hora_encendido, minutos_encendido, tiempo.hours, tiempo.minutes);

   //Leemos la temperatura en ese momento
   temperatura = ds1820_read();

   //Descomponemos la temperatura en su parte entera y parte decimal para almacenarla
   t_entera = (int)temperatura;
   t_decimal = (int)((temperatura-t_entera)*100);

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
      write_eeprom(eeprom_anno_actual, anno_actual);
      write_eeprom(eeprom_anno_0_to_3, anno_actual_0_to_3);
   }


   //Almacenamos todos los datos requeridos
   write_eeprom(eeprom_registros + (num_registros)*10    , tiempo.day);                  //Día de desconexión
   write_eeprom(eeprom_registros + (num_registros)*10 + 1, tiempo.month);                //Mes de desconexión
   write_eeprom(eeprom_registros + (num_registros)*10 + 2, anno_actual);                 //Año de desconexión
   write_eeprom(eeprom_registros + (num_registros)*10 + 3, tiempo.hours);                //Hora de desconexión
   write_eeprom(eeprom_registros + (num_registros)*10 + 4, tiempo.minutes);              //Minutos de desconexión
   write_eeprom(eeprom_registros + (num_registros)*10 + 5, t_entera);                    //Valor entero de última temperatura medida
   write_eeprom(eeprom_registros + (num_registros)*10 + 6, t_decimal);                   //Valor decimal de última temperatura medida
   write_eeprom(eeprom_registros + (num_registros)*10 + 7, (unsigned int)minutos_trans);          //Minutos transcurridos desde el encendido
   write_eeprom(eeprom_registros + (num_registros)*10 + 8, (unsigned int)(t_total_caldera/60));   //Tiempo total que ha estado encendida la caldera
                                                                                                 //en este intervalo (en minutos)

   //Guardamos también el último valor del termostato, teniendo en cuenta la histéresis del sistema
   if(temp_habitacion_superada)
      write_eeprom(eeprom_registros + (num_registros)*10 + 9, (unsigned int)(termostato+histeresis_vivienda));
   else
      write_eeprom(eeprom_registros + (num_registros)*10 + 9, (unsigned int)termostato);

   //Aumentamos en uno el número de registros
   num_registros++;

   //Guardamos el número de registros en memoria
   write_eeprom(eeprom_num_registros, num_registros);

   //Finalmente, apagamos el sistema
   sistema_encendido = FALSE;
}

//------------------------------------------------------------------------------------------------------------------------------------

int buscar_numero(int led_unidades, int min, int max, int valor_inicial){
   signed int input;
   unsigned int32 contador;

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
         //Además introducimos un mecanismo de avance rápido que permite pasar de un
         //valor a otro rápidamente sin más que mantener pulsada la tecla
         contador = 1;
         while(!mas)
         {
          if(contador>40000)
          {
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
            //Introducimos un poco de retraso
            delay_ms(200);
          }else
            contador++;
         }
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
         //Además introducimos un mecanismo de avance rápido que permite pasar de un
         //valor a otro rápidamente sin más que mantener pulsada la tecla
         contador = 1;
         while(!menos){
          if(contador>40000)
          {
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
            //Introducimos un poco de retraso
            delay_ms(200);
          }else
            contador++;
         }
         continue;
      }
      if(!si)
      {
         //Esperamos a que se suelte la tecla
         while(!si){}
         break;
      }
      if(!no)
      {
         //Si se pulsa No, devolvemos el valor correspondiente
         input = NOCODE;
         //Esperamos...
         while(!no){}
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
   while(!isdigit(c[0]) && c[0]!='\b')
      c[0]=getch();
   //Si se trata de un "backspace", devolvemos un mensaje de anulación de operación
   if(c[0]=='\b')
      return NOCODE;
   //Imprimimos la primera cifra
   printf("%c", c[0]);

   //Leemos la segunda cifra del número, procediendo de la misma manera
   while(!isdigit(c[1]) && c[1]!='\b')
      c[1]=getch();
   if(c[1]=='\b')
      return NOCODE;
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

int comprobar_hora(int hora1, int min1, int hora2, int min2)
{
   //Si hora1 es menor, devolvemos TRUE
   if(hora1 < hora2)
      return TRUE;
   //Si hora1 es mayor, devolvemos FALSE
   if(hora1 > hora2)
      return FALSE;
   //Si las horas coinciden habrá que comparar los minutos
   if(hora1==hora2)
   {
      //Se sigue el mismo criterio de antes con la comparación
      //de los minutos.
      if(min1 <= min2)
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

long diferencia_tiempo(int hora1, int min1, int hora2, int min2)
{
   long dif;

   //Si hora1:min1 es menor que hora2:min2, devolvemos simplemente la diferencia en minutos
   if(comprobar_hora(hora1,min1,hora2,min2))
      dif = ((long)hora2*60 + (long)min2) - ((long)hora1*60 + (long)min1);
   //Si no es así, habrá que tener en cuenta que hora1:min1 se encuentra en el día anterior
   //al de hora2:min2
   else
      dif = (24*60-((long)hora1*60 + (long)min1)) + (long)hora2*60 + (long)min2;

   //Devolvemos el resultado
   return dif;
}

//------------------------------------------------------------------------------------------------------------------------------------

long diferencia_tiempo_sec(int hora1, int min1, int sec1, int hora2, int min2, int sec2)
{
   unsigned long dif;
   unsigned long dif2;

   //Si hora1:min1 es menor que hora2:min2, ambas horas están en el mismo día
   if(comprobar_hora(hora1,min1,hora2,min2))
   {
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
   }
   else{
      //Si no, simplemente tenemos en cuenta que están en días diferentes y luego le añadimos la
      //diferencia en segundos
      dif2 = (24*60-((long)hora1*60 + (long)min1)) + (long)hora2*60 + (long)min2;
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

void grabar_programaciones()
{
   programacion pr; //Variable temporal de programacion
   int contador;   //Contador para el bucle

   //Recorremos todo el vector de programaciones (hasta el número de programaciones que tengamos)
   for(contador=0; contador<num_intervalos; contador++)
   {
      //Seleccionamos la programación
      pr = programaciones[contador];

      //Guardamos en la memoria EEPROM dato a dato
      write_eeprom(eeprom_programaciones + (contador)*5    , pr.horas_inicio);
      write_eeprom(eeprom_programaciones + (contador)*5 + 1, pr.minutos_inicio);
      write_eeprom(eeprom_programaciones + (contador)*5 + 2, pr.horas_fin);
      write_eeprom(eeprom_programaciones + (contador)*5 + 3, pr.minutos_fin);
      write_eeprom(eeprom_programaciones + (contador)*5 + 4, pr.termostato);
   }

   //Guardamos en la EEPROM en el número de intervalos
   write_eeprom(eeprom_num_intervalos, num_intervalos);
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
   printf("\rInicializacion del sistema\r");
   printf("**************************\r");
   //Primero solicitamos el día de la semana
   printf("Dia de la semana (1->Lunes, 2->Martes, ...): LUNES    ");

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
      //anterior. Para ello, todos los días ocupan el tamaño del mayor (miércoles) gracias a espacios.
      switch(weekday)
      {
         case 0: printf("\b\b\b\b\b\b\b\b\bLUNES    ");
                 break;
         case 1: printf("\b\b\b\b\b\b\b\b\bMARTES   ");
                 break;
         case 2: printf("\b\b\b\b\b\b\b\b\bMIERCOLES");
                 break;
         case 3: printf("\b\b\b\b\b\b\b\b\bJUEVES   ");
                 break;
         case 4: printf("\b\b\b\b\b\b\b\b\bVIERNES  ");
                 break;
         case 5: printf("\b\b\b\b\b\b\b\b\bSABADO   ");
                 break;
         case 6: printf("\b\b\b\b\b\b\b\b\bDOMINGO  ");
      }
   }

   //Pedimos la introducción de la fecha
   printf("\rFECHA: ");

   //Leemos el día del mes
   dia = buscar_numero_rs232();
   //Si pulsamos "backspace", se resetea la inicialización
   if(dia==NOCODE)
   {
      printf("\rReseteando inicializacion...\r");
      goto e1;
   }
   //Si el día introducido no es válido, también reseteamos la inicialización
   if(dia>31 || dia==0)
   {
      printf("\rDia incorrecto. Reseteando inicializacion...\r");
      delay_ms(LCD_T_RETARDO);
      goto e1;
   }

   //Leemos el mes
   printf("/");
   mes = buscar_numero_rs232();
   //Si pulsamos "backspace", se resetea la inicialización
   if(mes==NOCODE)
   {
      printf("\rReseteando inicializacion...\r");
      goto e1;
   }
   //Si el mes introducido no es válido, reseteamos la inicialización de igual modo
   if(mes>12 || mes==0)
   {
      printf("\rMes incorrecto. Reseteando inicializacion...\r");
      delay_ms(LCD_T_RETARDO);
      goto e1;
   }

   //Si el día introducido no se encuentra en el mes seleccionado, mostramos un mensaje
   //de error y reseteamos la operación
   if((dia>29 && mes==2)||(dia==31 && (mes==4 || mes==6 || mes==9 || mes==11)))
   {
      printf("\rError en la fecha. Reseteando inicializacion...\r");
      delay_ms(LCD_T_RETARDO);
      goto e1;
   }

   //Leemos el año
   printf("/");
   anno = buscar_numero_rs232();
   //Si pulsamos "backspace", se resetea la inicialización
   if(anno==NOCODE)
   {
      printf("\rReseteando inicializacion...\r");
      delay_ms(LCD_T_RETARDO);
      goto e1;
   }
   //Si el valor del año es incorrecto, reseteamos también la inicialización.
   //Tampoco se permite un año menor que 2009 (año de fabricación del sistema de control)
   if(anno>99 || anno<9)
   {
      printf("\rAnno incorrecto. Reseteando inicializacion...\r");
      delay_ms(LCD_T_RETARDO);
      goto e1;
   }


   //Si el año no es bisiesto y hemos seleccionado el 29 de febrero,
   //mostramos un mensaje y reseteamos la operación.
   if(anno%4!=0 && mes==2 && dia==29)
   {
      printf("\rEl anno no es bisiesto. Reseteando inicializacion...\r");
      delay_ms(LCD_T_RETARDO);
      goto e1;
   }

   //Introducimos un pequeño retardo para que el técnico vea los valores introducidos
   delay_ms(LCD_T_RETARDO);

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

   e2:
   //Se pide la introducción de la hora
   printf("\rHORA: ");

   //Leemos la hora
   horas = buscar_numero_rs232();
   //Si se pulsa "backspace", se resetea todo el proceso
   if(horas==NOCODE)
   {
      printf("\rReseteando la inicializacion...\r");
      delay_ms(LCD_T_RETARDO);
      goto e1;
   }
   //Si las horas son incorrectas, mostramos un mensaje de error por pantalla y
   //reseteamos la introducción de la misma.
   if(horas>23)
   {
      printf("  Hora incorrecta!");
      delay_ms(LCD_T_RETARDO);
      goto e2;
   }


   //Leemos los minutos
   printf(":");
   minutos = buscar_numero_rs232();

   //Si se pulsa "backspace", reseteamos la inicialización completa
   if(minutos==NOCODE)
   {
      printf("\rReseteando la inicializacion...\r");
      delay_ms(LCD_T_RETARDO);
      goto e1;
   }
   //Si los minutos no son correctos, mostramos un mensaje de error por pantalla y
   //reseteamos la introducción de los minutos
   if(minutos>59)
   {
      printf("  Minutos incorrectos!");
      delay_ms(LCD_T_RETARDO);
      goto e2;
   }

   //Introducimos un pequeño retardo para que el técnico pueda ver la hora introducida
   delay_ms(LCD_T_RETARDO);

   //Mostramos un mensaje de guardado de configuración
   printf("\rGuardando configuracion...");

   //Almacenamos todo en la estructura que contiene la información del tiempo
   tiempo.month   = mes;
   tiempo.day     = dia;
   tiempo.year    = anno_actual_0_to_3;
   tiempo.hours   = horas;
   tiempo.minutes = minutos;
   tiempo.seconds = 0x00;
   tiempo.weekday = weekday;

   //Fijamos el reloj a esa fecha y hora
   PCF8583_set_datetime(&tiempo);

   //Establecemos los parámetros iniciales del sistema para resetear la memoria
   termostato = (float)20;
   num_intervalos = 0;
   num_registros = 0;
   anno_actual = anno;

   //Escribimos los datos en la EEPROM
   write_eeprom(eeprom_termostato, (int)termostato);
   write_eeprom(eeprom_num_intervalos, num_intervalos);
   write_eeprom(eeprom_num_registros, num_registros);
   write_eeprom(eeprom_anno_actual, anno_actual);
   write_eeprom(eeprom_anno_0_to_3, anno_actual_0_to_3);

   //Escribimos el valor del termostato en el sensor de temperatura.
   //Ponemos como valor alto 127.5 grados, temperatura que no se va a alcanzar.
   ds1820_establecer_TH_TL(127.5, termostato);

   //Mostramos un mensaje de inicio del sistema
   printf("\rIniciando sistema...\r");
}

//------------------------------------------------------------------------------------------------------------------------------------

void leer_programaciones(){
   programacion pr;  //Variable temporal para guardar la programación
   int contador;    //Contador del bucle

   //Se supone que antes ha sido recuperado el número de intervalos
   //Recorremos todas las programaciones guardadas
   for(contador=0; contador<num_intervalos; contador++)
   {
      //Leemos de memoria y vamos guardando los datos correspondientes
      pr.horas_inicio   = read_eeprom(eeprom_programaciones + (contador)*5);
      pr.minutos_inicio = read_eeprom(eeprom_programaciones + (contador)*5 + 1);
      pr.horas_fin      = read_eeprom(eeprom_programaciones + (contador)*5 + 2);
      pr.minutos_fin    = read_eeprom(eeprom_programaciones + (contador)*5 + 3);
      pr.termostato     = read_eeprom(eeprom_programaciones + (contador)*5 + 4);

      //Guardamos la programación en el vector correspondiente
      programaciones[contador] = pr;

   }
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
   printf(lcd_putc, "TEMP - %3.1f %cC\nHORA - ", temperatura, 223);

   //Mostramos la hora
   //Comprobamos si los valores son menores que 10 para añadir un cero
   //delante y que queden los valores alineados
   if(tiempo.hours < 10)
      printf(lcd_putc, "0");
   printf(lcd_putc, "%u:", tiempo.hours);
   if(tiempo.minutes < 10)
      printf(lcd_putc, "0");
   printf(lcd_putc, "%u", tiempo.minutes);
}

//------------------------------------------------------------------------------------------------------------------------------------

void ordenar_programaciones()
{
   //Variables temporales y contadores auxiliares
   programacion *pr1;
   programacion *pr2;
   int contador1, contador2;
   int h_inicio_1, min_inicio_1, h_inicio_2, min_inicio_2;

   //Ambos punteros comienzan apuntando al principio de programaciones
   pr1 = programaciones;
   pr2 = programaciones;


   //Algoritmo de la burbuja
   for(contador1=0; contador1 < num_intervalos; contador1++)
   {
      //Apuntamos a las dos últimas posiciones del vector
      pr1 = programaciones+num_intervalos-2;
      pr2 = programaciones+num_intervalos-1;

      //Iteramos hasta llegar a la zona ordenada del vector
      for(contador2=0; contador2 < num_intervalos-contador1-1; contador2++)
      {
         //Tomamos para la comparación los instantes de inicio
         h_inicio_1   = (*pr1).horas_inicio;
         min_inicio_1 = (*pr1).minutos_inicio;
         h_inicio_2   = (*pr2).horas_inicio;
         min_inicio_2 = (*pr2).minutos_inicio;

         //Si la hora de pr2 es menor que la de pr1, habrá que intercambiar los
         //valores para mover la menor a una posición más baja
         if(comprobar_hora(h_inicio_2, min_inicio_2, h_inicio_1, min_inicio_1))
         {
            programacion temporal;
            temporal = *pr1;
            *pr1 = *pr2;
            *pr2 = temporal;
         }

         //Pasamos a la siguiente posición y repetimos el proceso
         pr1--;
         pr2--;
      }

   }

}

//------------------------------------------------------------------------------------------------------------------------------------

void programar_proxima_alarma()
{
   //Recuperamos los datos de inicio de la alarma actual
   prg=programaciones[posicion_alarmas];

   //Establecemos la alarma a ese tiempo
   hora_alarma = prg.horas_inicio;
   minutos_alarma = prg.minutos_inicio;
   alarma = TRUE;
}

//------------------------------------------------------------------------------------------------------------------------------------

void representar_registros()
{
   //Contadores para los bucles
   unsigned int contador, contador2;
   //Caracter para la introducción de las opciones
   char caracter;
   //Lista de variables recuperadas
   unsigned int dia, mes, anno, horas, minutos, temp_int, temp_dec, t_sistema, t_caldera, term;

   float media_dia_mes[12];         //Tiempos medios en minutos que se enciende el sistema en cada mes
   int dias_meses[12];              //Número de dias que tiene cada mes
   float media_dia_caldera;         //Tiempo medio que se enciende la caldera cada día
   float valor;                     //Tiempo medio en minutos que se enciende el sistema por día
   float media_dia_mes_caldera[12]; //Tiempo medio que se enciende la caldera en cada mes

   unsigned long valor_total;  //Tiempo total de encendido de la caldera
   unsigned long valor_total_caldera; //Tiempo total de encendido del sistema

   //Capacidad para 4 años de datos
   //Vectores de almacenamiento de los datos anuales
   float media_annos[4][12];
   float media_annos_caldera[4][12];
   //Número de años de los que se tienen datos
   int num_annos;
   //Años correspondientes
   int annos[4];

   //Variables para almacenar valores provisionales
   long tiempo_x_dia;         //Tiempo que se enciende cada día el sistema (minutos)
   long tiempo_x_dia_caldera; //Tiempo que se enciende cada día la caldera (minutos)
   int fecha[3];             //Día con el que estamos trabajando (día/mes/año)
   long num_dias;             //Número de días que llevamos analizados

   //dias_meses = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
   dias_meses[0] = dias_meses[2] = dias_meses[4] = dias_meses[6] = dias_meses[7] = dias_meses[9] = dias_meses[11] = 31;
   dias_meses[1] = 28;
   dias_meses[3] = dias_meses[5] = dias_meses[8] = dias_meses[10] = 30;

   //Inicializamos los vectores a 0
   for(contador=0; contador<12;contador++)
   {
      media_dia_mes[contador]=0;
   }

   for(contador=0; contador<12;contador++)
   {
      media_dia_mes_caldera[contador]=0;
   }

   for(contador=0; contador<4 ;contador++)
   {
      for(contador2=0; contador2<12 ;contador2++)
      {
         media_annos[contador][contador2]=0;
      }
   }

   for(contador=0; contador<4 ;contador++)
   {
      for(contador2=0; contador2<12 ;contador2++)
      {
         media_annos_caldera[contador][contador2]=0;
      }
   }

   //Iniciamos a cero las otras variables
   fecha[0] = fecha[1] = fecha[2] = 0;
   num_dias = 0;
   num_annos = 0;

   media_dia_caldera = 0;
   valor = 0;
   valor_total = 0;
   valor_total_caldera = 0;

   //Imprimimos el encabezamiento
   printf("\rREGISTRO DE EVENTOS\r");
   printf("*******************\r");
   printf("Fecha           Temp.    Term.  Min.S.  Min.C.\r");
   printf("==============================================\r");

   //Recorremos todos los registros almacenados
   for(contador=0; contador<num_registros; contador++)
   {
      //Comenzamos leyendo los datos de memoria
      dia         =  read_eeprom(eeprom_registros + contador*10);
      mes         =  read_eeprom(eeprom_registros + contador*10 + 1);
      anno        =  read_eeprom(eeprom_registros + contador*10 + 2);
      horas       =  read_eeprom(eeprom_registros + contador*10 + 3);
      minutos     =  read_eeprom(eeprom_registros + contador*10 + 4);
      temp_int    =  read_eeprom(eeprom_registros + contador*10 + 5);
      temp_dec    =  read_eeprom(eeprom_registros + contador*10 + 6);
      t_sistema   =  read_eeprom(eeprom_registros + contador*10 + 7);
      t_caldera   =  read_eeprom(eeprom_registros + contador*10 + 8);
      term        =  read_eeprom(eeprom_registros + contador*10 + 9);

      //Si cambiamos de día debemos gestionar los datos del nuevo día y almacenar los del anterior
      if(fecha[0]!=dia || fecha[1]!=mes || fecha[2]!=anno)
      {
         //Lógicamente, no tenemos en cuenta para la media los días en los que no se enciende:
         //días de verano, calurosos, etc. Esta información puede visualizarse en la media
         //por meses.
         //Guardamos los datos del día anterior si el día no es el primero
         if(fecha[0]!=0)
         {
            //En la media de tiempo por día, acumulamos el tiempo que ha estado encendido el último día
            valor = valor + (float)tiempo_x_dia;
            //Guardamos también este valor en el registro del año
            media_dia_mes[fecha[1]-1] = valor;
            //Realizamos la misma operación con la media de encendido de la caldera
            media_dia_caldera = media_dia_caldera + (float)tiempo_x_dia_caldera;
            media_dia_mes_caldera[fecha[1]-1] = media_dia_caldera;
         }
         //La sumatoria podría alcanzar un valor muy alto. El sistema no está preparado para estas eventualidades
         //y debería ser reseteado cada cierto tiempo. Si el sistema va a utilizarse mucho (zonas muy frías o mal
         //aisladas), el usuario debería informar al fabricante para aumentar la memoria de almacenamiento y el
         //tamaño de las variables

         //Si cambiamos de año, habrá que guardar los datos anuales
         if(fecha[2]!=anno)
         {
            //Comprobamos que no nos encontrabamos en el año introducido por defecto
            if(fecha[2]!=0)
            {
               //En ese caso, guardamos los datos de todo el año
               for(contador2=0; contador2<12; contador2++)
               {
                  media_dia_mes[contador2] = media_dia_mes[contador2]/(float)dias_meses[contador2];
                  media_dia_mes_caldera[contador2] = media_dia_mes_caldera[contador2]/(float)dias_meses[contador2];
                  media_annos[num_annos][contador2] = media_dia_mes[contador2];
                  media_annos_caldera[num_annos][contador2] = media_dia_mes_caldera[contador2];
                  //Inicializamos de nuevo los vectores que almacenan los datos del año actual
                  media_dia_mes[contador2]=0;
                  media_dia_mes_caldera[contador2]=0;
               }

               //Guardamos el año del que se trata
               annos[num_annos]=fecha[2];

               //Aumentamos en 1 el número de años
               num_annos++;
            }

            //COMPROBAR BISIESTO
            //En realidad, un año será bisiesto si es divisible por 4 y no lo es por 100,
            //excepto si este último lo es por 400. Sin embargo, dado que trabajamos con las
            //dos últimas cifras y que el próximo año divisible por 4 que no lo es por 100
            //es el 2100, mejor nos limitaremos a la divisibilidad por 4.
            //Dependiendo si el año es bisiesto o no, le damos a febrero 29 o 28 días.
            if(anno%4 == 0)
               dias_meses[1] = 29;
            else
               dias_meses[1] = 28;
         }

         //Modificamos la información del día
         fecha[0] = dia;
         fecha[1] = mes;
         fecha[2] = anno;

         //Aumentamos en 1 el número de días
         num_dias++;

         //Reseteamos las variables acumulativas de tiempo por día
         tiempo_x_dia = 0;
         tiempo_x_dia_caldera = 0;
      }

      //Le sumamos al tiempo de encendido de sistema y caldera el correspondiente de este registro
      tiempo_x_dia = tiempo_x_dia + t_sistema;
      tiempo_x_dia_caldera = tiempo_x_dia_caldera + t_caldera;
      //También incrementamos los tiempos totales
      valor_total = valor_total + t_sistema;
      valor_total_caldera = valor_total_caldera + t_caldera;

      //Representamos todos los datos, teniendo en cuenta algunos requisitos representativos
      //para que los datos queden alineados (número menores que 10, que 100, etc).
      if(dia<10)
         printf(" ");

      printf("%u/", dia);

      if(mes<10)
         printf("0");

      printf("%u/", mes);

      if(anno<10)
         printf("0");

      printf("%u  ", anno);

      if(horas<10)
         printf("0");

      printf("%u:", horas);

      if(minutos<10)
         printf("0");

      printf("%u ", minutos);

      if(temp_int<10)
         printf(" ");

      printf("%u.", temp_int);

      if(temp_dec<10)
         printf("0");

      printf("%u%cC  %u%cC   ", temp_dec, 223, term, 223);

      if(t_sistema<100)
         printf(" ");
      if(t_sistema<10)
         printf(" ");

      printf("%u     ", t_sistema);

      if(t_caldera<100)
         printf(" ");
      if(t_caldera<10)
         printf(" ");

      printf("%u\r", t_caldera);

   }

   //Debemos realizar la operación de guardado para el último día. El proceso es el mismo
   //que hemos seguido arriba.
   if(fecha[0]!=0)
   {
      valor = valor + (float)tiempo_x_dia;
      media_dia_mes[fecha[1]-1] = valor;
      media_dia_caldera = media_dia_caldera + (float)tiempo_x_dia_caldera;
      media_dia_mes_caldera[fecha[1]-1] = media_dia_caldera;
   }

   //Comprobamos que no nos encontrabamos en el año introducido por defecto
   if(fecha[2]!=0)
   {
      for(contador2=0; contador2<12; contador2++)
      {
         media_dia_mes[contador2] = media_dia_mes[contador2]/(float)dias_meses[contador2];
         media_dia_mes_caldera[contador2] = media_dia_mes_caldera[contador2]/(float)dias_meses[contador2];
         media_annos[num_annos][contador2] = media_dia_mes[contador2];
         media_annos_caldera[num_annos][contador2] = media_dia_mes_caldera[contador2];
      }

      annos[num_annos]=fecha[2];

      num_annos++;
   }

   //Calculamos la media como el cociente entre los valore de tiempo calculados y el número de
   //días en los que se ha encendido la caldera
   valor = valor/(float)num_dias;
   media_dia_caldera = media_dia_caldera/(float)num_dias;

   repetir:
   //Menú para mostrar los datos
   printf("\r=============================================\r");
   printf("¿Que operacion desea realizar?\r");
   printf("0. Salir\r");
   printf("1. Extraer tiempos medios por dia\r");
   printf("2. Extraer tiempos totales\r");
   printf("3. Extraer datos mensuales\r");
   printf("=============================================\r");

   //Esperamos la introducción de una de las opciones por parte del técnico.
   //Cuando se pulse uno de los botones, asignamos a la variable caracter el valor correspondiente.
   caracter='J';
   while(caracter!='0' && caracter!='1' && caracter!='2' && caracter!='3'){
      caracter=getch();
   }

   switch(caracter)
   {
      //En el caso 0, se sale del sistema
      case '0':
               //Se indica por pantalla la salida
               printf("\rSaliendo...\r");
               break;

      //En el caso 1, se muestran los valores medios
      case '1':
               printf("\rT. medio encendido sistema por dia: %3.2f minutos\r\r", valor);
               printf("T. medio encendido caldera por dia: %3.2f minutos\r\r", media_dia_caldera);
               goto repetir;
               break;

      //En el caso 2, se muestran los valores totales
      case '2':
               printf("\rT. total encendido sistema: %lu minutos\r\r", valor_total);
               printf("T. total encendido caldera: %lu minutos\r\r", valor_total_caldera);
               goto repetir;
               break;

      //En el caso 3, se muestran todos los valores medios por meses y años
      case '3':
               //Se representan todos los años de los que se tiene registro
               for(contador=0; contador<num_annos; contador++)
               {
                  //Imprimimos el año en el que nos encontramos
                  printf("\r20");
                  if(annos[contador]<10)
                     printf("0");
                  printf("%u\r", annos[contador]);
                  printf("-----------------------------------------------------\r");
                  printf("           T. sistema (min/dia)  T. caldera (min/dia)\r");
                  printf("ENERO              %3.2f                %3.2f\r", media_annos[contador][0], media_annos_caldera[contador][0]);
                  printf("FEBRERO            %3.2f                %3.2f\r", media_annos[contador][1], media_annos_caldera[contador][1]);
                  printf("MARZO              %3.2f                %3.2f\r", media_annos[contador][2], media_annos_caldera[contador][2]);
                  printf("ABRIL              %3.2f                %3.2f\r", media_annos[contador][3], media_annos_caldera[contador][3]);
                  printf("MAYO               %3.2f                %3.2f\r", media_annos[contador][4], media_annos_caldera[contador][4]);
                  printf("JUNIO              %3.2f                %3.2f\r", media_annos[contador][5], media_annos_caldera[contador][5]);
                  printf("JULIO              %3.2f                %3.2f\r", media_annos[contador][6], media_annos_caldera[contador][6]);
                  printf("AGOSTO             %3.2f                %3.2f\r", media_annos[contador][7], media_annos_caldera[contador][7]);
                  printf("SEPTIEMBRE         %3.2f                %3.2f\r", media_annos[contador][8], media_annos_caldera[contador][8]);
                  printf("OCTUBRE            %3.2f                %3.2f\r", media_annos[contador][9], media_annos_caldera[contador][9]);
                  printf("NOVIEMBRE          %3.2f                %3.2f\r", media_annos[contador][10], media_annos_caldera[contador][10]);
                  printf("DICIEMBRE          %3.2f                %3.2f\r", media_annos[contador][11], media_annos_caldera[contador][11]);
                  printf("-----------------------------------------------------\r");
               }

               goto repetir;
               break;

      //En los tres casos, vuelve a mostrarse el menú tras imprimir las estadísticas
   }
}

//------------------------------------------------------------------------------------------------------------------------------------

void seleccionar_alarma()
{
   //Contador y variables auxiliares
   int contador;
   int hora, minutos, hora_p, min_p;

   //Ponemos la posición en el vector de alarmas incialmente a 0
   posicion_alarmas = 0;

   //Leemos el tiempo actual
   PCF8583_read_datetime(&tiempo);
   hora = tiempo.hours;
   minutos = tiempo.minutes;

   //Recorremos todas las programaciones
   for(contador=0; contador<num_intervalos; contador++)
   {
       hora_p = programaciones[contador].horas_inicio;
       min_p  = programaciones[contador].minutos_inicio;

       //Comprobamos las horas y tomamos la primera programación que tenga una
       //hora mayor que la actual
       if(!comprobar_hora(hora_p, min_p, hora, minutos))
       {
           //Guardamos la posición de esta programación y salimos
           posicion_alarmas = contador;
           break;
       }
   }
}
