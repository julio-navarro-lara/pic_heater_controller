//*****************************************************************
//***   PROYECTO 1: Control de un sistema de calefacción de una ***
//***   vivienda utilizando un microcontrolador PIC18F4520      ***
//*****************************************************************
//Author: Julio Navarro Lara        2010



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
#include "teclado.c"
#include "eeprom.c"


#use fast_io(A)
#use fast_io(B)
#use fast_io(C)
#use fast_io(D)

int1 temperatura_caldera_superada; //Control de la histéresis en la caldera
int1 temp_habitacion_superada1;  //Control de la histéresis del ambiente
int1 temp_habitacion_superada2;
int1 encendido_por_alarma; //Indica si la última vez que se encendió fue en respuesta o no a una alarma
int1 toca_encender; //Indica si la próxima alarma es de encendido o de apagado
float termostato1; //Temperatura límite de la zona 1 de la vivienda
float termostato2; //Temperatura límite de la zona 2 de la vivienda
float termostato_provisional; //Valor de termostato provisional para cuando saltan las alarmas
float temperatura1; //Temperatura de la zona 1 de la vivienda
float temperatura2; //Temperatura de la zona 2 de la vivienda
float histeresis_vivienda = 0.5; //Intervalo de histéresis de la vivienda
char key; //Tecla a pulsar
char keys[2]; //Cadena para almacenar números como caracteres

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
programacion programaciones[5];

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
unsigned long num_registros;  //Número de registros de fechas almacenados
int anno_actual;             //Año en el que estamos (de 0 a 99)
int anno_actual_0_to_3;      //Año en el que estamos (de 0 a 3)

//Flag para activar las interrupciones de interfaz con el teclado y alarma
int1 led_int;
int1 alarma_int;
//Flag para controlar el desbordamiento del TIMER1
int1 timer_int;

//Arrays para almacenar las direcciones de las ROM de ambos sensores de temperatura
int dir1[8], dir2[8];

//Contador para medir el tiempo transcurrido con el menú activo
unsigned int contador_tiempo;


//Listado de métodos utilizados ordenados por orden alfabético
//************************************************************
//Mostramos aquí una breve explicación de cada uno. El interior de los métodos está
//descrito después del método main.

//apagar_caldera() -> Apaga la caldera, almacenando el tiempo que ha estado encendida.
void apagar_caldera();
//apagar_sistema() -> Realiza todas las operaciones necesarias para el apagado
//del sistema, como el almacenamiento de los datos necesarios en memoria.
void apagar_sistema();
//buscar_numero() -> Escanea el teclado hasta encontrar un número de dos cifras o hasta que se decida salir
//mediante la pulsación de los botones de SI o NO.
int buscar_numero();
//buscar_numero_rs232() -> Registra la introducción de un número de dos cifras mediante la interfaz RS232 y
//lo devuelve. Además, si se pulsa el "backspace" durante el proceso, devuelve el código correspondiente a NO
int buscar_numero_rs232();
//comp_caldera() -> Devuelve TRUE si debemos apagar la caldera. Esta función tiene en cuenta
//cierta histéresis para evitar ciclos continuos de apagado-encendido. Hay que considerar
//que sólo puede emplearse una vez que la temperatura del agua ha superado ya el valor necesario.
int comp_caldera();
//comprobar_hora(int hora1, int min1, int hora2, int min2) -> Devuelve TRUE
//si hora1:min1 corresponde a un tiempo menor o igual que hora2:min2.
int comprobar_hora(int hora1,int min1, int hora2, int min2);
//comprobar_temperatura() -> Devuelve un código según la temperatura de los dos sensores:
//0: Ninguno de los dos supera la temperatura del termostato
//1: El sensor 1 supera la temperatura del termostato
//2: El sensor 2 supera la temperatura del termostato
//3: Ambos sensores superan la temperatura del termostato
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
//inicializacion() -> Lleva a cabo la inicialización del sistema (introducción de hora y fecha,
//puesta a punto de variables, etc)
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

//Gestión de interrupciones de activación de la alarma
#INT_EXT
void alarma(){
   //Se activa el flag correspondiente
   alarma_int = TRUE;
}

//Gestión de interrupciones de activación de la interfaz de usuario
#INT_EXT1
void activacion_led(){
   //Se activa el flag correspondiente
   led_int = TRUE;
}

//Interrupción de reloj para controlar el tiempo que está activo el menú
#INT_TIMER1
void contador(){
   //Aumentamos en uno el valor del contador
   contador_tiempo++;
   //Si llega a 76 (unos 20 segundos), activamos el flag correspondiente
   if(contador_tiempo >= 76)
      timer_int = TRUE;
}


//***********************************************************
//Método main
//***********************************************************
void main()
{
   //Vector para recoger los datos iniciales leídos de memoria
   int data[6];

   //Valor auxiliar
   int valor;

   //Inicializamos salidas
   //Comenzamos con todo apagado (sistema, motor y caldera)
   sistema_encendido = FALSE;
   motor1 = FALSE;
   motor2 = FALSE;
   caldera_encendida = FALSE;
   encendido_por_alarma = FALSE;
   t_total_caldera = 0; //El tiempo total de encendido de la caldera también lo ponemos a 0

   //Establecemos el estado de los puertos como entradas o salidas
   set_tris_b(0x03);
   set_tris_d(0x0F);


   //Inicializamos la pantalla lcd
   lcd_init();

   //Mostramos un mensaje de inicio
   lcd_send_byte(LCD_ORDEN, LCD_CLEAR);
   lcd_send_byte(LCD_ORDEN, LCD_NO_CURSOR);
   printf(lcd_putc, "Inicializando...");

   /*
   NOTA: En el guión se nos pide que se realice la inicialización del sistema cuando
   éste se instala, introduciendo fecha y hora y reseteando los datos de memoria para
   iniciar el funcionamiento. Esto lo hace en nuestro programa la funcion inicializar.
   Sin embargo, se ha preferido, dado que PROTEUS actualiza su reloj con el del ordenador,
   dar la opción de dejar inhabilitada esta rutina para así disponer de datos históricos guardados en la
   EEPROM y poder analizarlos. Debido a que nuestra simulación no puede estar las 24 horas
   funcionando (entre otras cosas, porque el PCF se para) es la única forma de simular
   que el sistema está siempre encendido.
      De todas formas, ambas rutinas están implementadas con plena operatividad. Si se quiere
   inicializar el sistema como ocurriría en la vida real, simplemente dejar descomentadas las dos
   líneas de código que siguen a este comentario y mantener comentado el trozo de código que está
   enmarcado por las ristras de arrobas (@), un poco más abajo.
   */


   inicializacion();
   PCF8583_write_byte(PCF8583_CTRL_STATUS_REG, PCF8583_START_COUNTING);


   //Recogemos las direcciones de los sensores de temperatura
   ds1820_recoger_direcciones(dir1, dir2);

   //Propiedades de conversión analógico digital
   //Establecemos el comparador entre A0 y A3, y entre A1 y A2
   setup_comparator(A0_A3_A1_A2);
   //Caracterizamos los puertos A0 a A3 como entradas analógicas
   setup_adc_ports(AN0_TO_AN3);
   //Definimos el reloj de conversión
   setup_adc(adc_clock_div_32);

/*
   //@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@

   //Recuperación de datos de la memoria
   //Recuperamos a partir de la dirección eeprom_termostato los datos en
   //el siguiente orden: termostato, número de intervalos de alarma, número de registros
   //de encendido de la caldera, año actual en valores de 0 a 99 y año actual en valores de 0 a 3.
   lee_ee(eeprom_termostato, 6, data);
   termostato1 = termostato2 = (float)data[0];
   num_intervalos = data[1];
   //El número de registros está almacenado en un long y debemos realizar la conversión desde dos int
   num_registros = (long)data[2]*256 + (long)data[3];
   anno_actual = data[4];
   anno_actual_0_to_3 = data[5];

   //Lee las programaciones de alarmas guardadas en memoria
   leer_programaciones();

   //Comprobamos si hay alarmas programadas en memoria
   if(num_intervalos!=0)
   {
      //Si las hay, seleccionamos la siguiente y programamos su activación
      seleccionar_alarma();
      programar_proxima_alarma();
      toca_encender = TRUE;
   }else
   {
      //Si no, simplemente inciamos la cuenta del reloj
      PCF8583_write_byte(PCF8583_CTRL_STATUS_REG,
                              PCF8583_START_COUNTING);
   }

   //@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
*/

   //Leemos la temperatura de las dos zonas de la vivienda
   temperatura1 = ds1820_read(dir1);
   temperatura2 = ds1820_read(dir2);

   if(temperatura1 >= termostato1)
   {
      //Si supera el valor del termostato, lo indicamos con el flag
      temp_habitacion_superada1 = TRUE;
      //Tendremos que establecer un nuevo termostato teniendo en cuenta la histéresis de la temperatura
      termostato1 = termostato1 - histeresis_vivienda;
   }else
      //Si no se supera, se indica también
      temp_habitacion_superada1 = FALSE;

   if(temperatura2 >= termostato2)
   {
      //Si supera el valor del termostato, lo indicamos con el flag
      temp_habitacion_superada2 = TRUE;
      //Tendremos que establecer un nuevo termostato teniendo en cuenta la histéresis de la temperatura
      termostato2 = termostato2 - histeresis_vivienda;
   }else
      //Si no se supera, se indica también
      temp_habitacion_superada2 = FALSE;


   if(!C1OUT)
      //Si la temperatura del agua en la caldera supera el termostato, activamos el flag
      temperatura_caldera_superada = TRUE;
   else
      //Si no es así, lo desactivamos
      temperatura_caldera_superada = FALSE;

   //Desactivamos los flag de las interrupciones
   alarma_int = led_int = timer_int = FALSE;

   //Determinamos que la patilla de interrupción de la alarma se activa al pasar de alta a baja
   ext_int_edge(0, H_TO_L);

   //Configuramos las interrupciones del temporizador
   setup_timer_1(T1_INTERNAL | T1_DIV_BY_8);

   //Activamos las interrupciones del puerto B0 y B1
   enable_interrupts(INT_EXT);
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

      //Si la interrupción de alarma se activa, gestionamos el proceso
      if(alarma_int)
      {
         //Variables para almacenar el tiempo
         int hora, minutos;

         //Desactivamos las interrupciones para que no interfieran con la gestión de la alarma
         disable_interrupts(GLOBAL);

         //Desactivamos el flag de la alarma en el PCF8583
         PCF8583_write_byte(PCF8583_CTRL_STATUS_REG,
                                 PCF8583_START_COUNTING);

         if(toca_encender)
         {
            if(!sistema_encendido)
            {
               //Ejecutamos esto si se trata de una alarma de encendido y el sistema
               //está apagado.
               //Actualizamos el termostato, guardando el valor que había ya (teniendo en
               //cuenta la histéresis)
               if(temp_habitacion_superada1)
                  termostato_provisional = termostato1 + histeresis_vivienda;
               else
                  termostato_provisional = termostato1;

               termostato1 = termostato2 = prg.termostato;

               //Si la temperatura no está superada, desactivamos los flag por si acaso
               //lo estaba con el otro valor de termostato.
               if(ds1820_read(dir1) < termostato1)
                  temp_habitacion_superada1 = FALSE;
               if(ds1820_read(dir2) < termostato2)
                  temp_habitacion_superada2 = FALSE;

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
               //Lo hacemos mediante variables auxiliares para dividir mejor las tareas
               //y no condensar todo en una línea.
               hora = prg.horas_fin;
               minutos = prg.minutos_fin;

               delay_ms(250); //Retraso para evitar solapamiento de alarmas
                              //(que salte una dos veces seguidas o algo así)

               //Establecemos la alarma de apagado del sistema (segundos=0)
               PCF8583_establecer_alarma(hora, minutos, 0);

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
               termostato1 = termostato2 = termostato_provisional;
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
         PCF8583_write_byte(PCF8583_CTRL_STATUS_REG,
                                 PCF8583_ACTIVAR_ALARMA);

         //Desactivamos el flag de interrupción
         alarma_int = FALSE;

         //Volvemos a activar las interrupciones
         enable_interrupts(GLOBAL);
      }


      //Si se activa la interrupción de interfaz con el usuario, se gestiona el proceso
      if(led_int)
      {
         //Contadores
         int contador, contador2;
         //Variable para la conversión de los valores de teclado
         long numero;
         //Variables provisionales para buscar en la lista de programaciones
         programacion pr, pr2;

         //Desactivamos interrupciones externas
         disable_interrupts(GLOBAL);
         

         //Ponemos en key un caracter cualquiera para que no se corresponda
         //con ningún botón del teclado.
         key='J';

         //Mostramos un mensaje al usuario para que elija su opción
         lcd_send_byte(LCD_ORDEN,LCD_CLEAR);
         printf(lcd_putc, "  Indique la \n  operacion");

         //Activamos el temporizador para controlar el tiempo que el menú está activo,
         //poniendo el contador a 0.
         contador_tiempo = 0;
         enable_interrupts(INT_TIMER1);

         //Mientras que la tecla pulsada no sea una de las válidas, seguimos barriendo el teclado
         while(key!='1' && key!='2' && key!='3' && key!='4' && key!='N'){
            key=get_key();
            //Si el tiempo de espera consignado expira, es equivalente a pulsar la tecla NO para salir.
            if(timer_int){
               key='N';
               timer_int = FALSE; //Desactivamos flag
            }
         }

         //Volvemos a deshabilitar la interrupción que controla el tiempo de espera
         disable_interrupts(INT_TIMER1);

         //Se elige la opción pertinente
         switch(key)
         {
            //Si es 1, entramos en la opción de apagado y encendido manual del sistema
            case '1':
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
                        //Si los motores estaban en marcha, debemos apagarlos
                        //motor1 = FALSE;   /* Lo hacemos ya dentro de la rutina
                        //motor2 = FALSE;

                        //Si estaba encendido por alguna alarma debemos indicarlo
                        if(encendido_por_alarma)
                        {
                           //Recuperamos el valor del termostato
                           termostato1 = termostato2 = termostato_provisional;
                           encendido_por_alarma = FALSE;
                        }
                     }
                     break;


            //Si es 2, entramos en la opción de configuración del termostato
            case '2':

                     //Mostramos el valor del termostato actual
                     lcd_send_byte(LCD_ORDEN, LCD_CLEAR);
                     //Para considerar el valor del termostato hay que tener en cuenta el valor de la histéresis.
                     //El termostato de la vivienda es único y no depende de la habitación en la que nos encontremos.
                     if(temp_habitacion_superada1)
                        printf(lcd_putc, "Modificar\ntermostato %.0f%cC",termostato1+histeresis_vivienda,223);
                     else
                        printf(lcd_putc, "Modificar\ntermostato %.0f%cC",termostato1,223);

                     //Retardamos cierto tiempo para que el usuario vea correctamente los datos de la pantalla
                     delay_ms(LCD_T_RETARDO*2);

                     //Pedimos la introducción de nuevo valor
                     lcd_send_byte(LCD_ORDEN, LCD_CLEAR);
                     lcd_send_byte(LCD_ORDEN, LCD_CURSOR); //Activamos el cursor en la pantalla
                     printf(lcd_putc, "Nuevo valor:\n       %cC",223);
                     lcd_gotoxy(6,2);

                     //Recogemos el número de dos cifras introducido por el teclado
                     valor=buscar_numero();

                     //Si se pulsa la tecla SI o NO, salimos
                     if(valor==NO || valor==SI)
                        goto salir;
                     else
                     {
                        //Retardamos cierto tiempo para permitir al usuario visualizar el valor que ha introducido
                        delay_ms(LCD_T_RETARDO);

                        //Mostramos un mensaje de aplicación de cambios y desactivamos el cursor
                        lcd_send_byte(LCD_ORDEN, LCD_CLEAR);
                        lcd_send_byte(LCD_ORDEN, LCD_NO_CURSOR);
                        printf(lcd_putc, "  Aplicando\n  cambios...");

                        if(valor<=temp_max && valor>=temp_min)
                        {
                           //Si el valor está dentro de los límites propuestos, guardamos el valor nuevo

                           //Leemos la temperatura de la zona1 de la vivienda
                           temperatura1 = ds1820_read(dir1);
                           if(temperatura1 >= (float)valor)
                           {
                              //Si supera el valor del termostato, lo indicamos con el flag
                              temp_habitacion_superada1 = TRUE;
                              //Guardamos el termostato nuevo teniendo en cuenta la histeresis de la vivienda
                              termostato1 = (float)valor - histeresis_vivienda;
                           }else
                           {
                              //Si no se supera, se indica también
                              temp_habitacion_superada1 = FALSE;
                              //Se guarda el valor tal cual
                              termostato1 = (float)valor;
                           }

                           //Repetimos el proceso con la zona2 de la vivienda
                           temperatura2 = ds1820_read(dir2);
                           if(temperatura2 >= (float)valor)
                           {
                              //Si supera el valor del termostato, lo indicamos con el flag
                              temp_habitacion_superada2 = TRUE;
                              //Guardamos el termostato nuevo teniendo en cuenta la histeresis de la vivienda
                              termostato2 = (float)valor - histeresis_vivienda;
                           }else
                           {
                              //Si no se supera, se indica también
                              temp_habitacion_superada2 = FALSE;
                              //Se guarda el valor tal cual
                              termostato2 = (float)valor;
                           }

                           //Guardamos el nuevo valor del termostato en memoria
                           graba_ee(eeprom_termostato, 1, &valor);

                        }
                        else
                        {
                           //Si el valor es incorrecto, mostramos un mensaje de error
                           lcd_send_byte(LCD_ORDEN, LCD_CLEAR);
                           printf(lcd_putc, "   Valor\n   incorrecto");
                           delay_ms(LCD_T_RETARDO);
                        }
                     }

                     break;


            //Si es 3, entramos en la opción de configuración de las alarmas
            case '3':

                     //Mostramos el encabezamiento de la operación
                     lcd_send_byte(LCD_ORDEN, LCD_CLEAR);
                     printf(lcd_putc," Programar\n sistema");
                     delay_ms(LCD_T_RETARDO);

                     //Tenemos la posibilidad de introducir 5 intervalos de alarma.
                     //Se va guiando al usuario por la configuración de cada intervalo mediante el siguiente bucle
                     for(contador=0; contador<5; contador++)
                     {
                        //Se muestra el intervalo a configurar
                        lcd_send_byte(LCD_ORDEN, LCD_CLEAR);
                        printf(lcd_putc, " INTERVALO %i\n   :      :",contador+1);
                        lcd_gotoxy(2,2);
                        lcd_send_byte(LCD_ORDEN, LCD_CURSOR);

                        //Se pide al usuario el valor de hora de inicio de la alarma
                        pr.horas_inicio = buscar_numero();

                        //Si se pulsa el botón de NO, se sale de la configuración
                        if(pr.horas_inicio==NO)
                           goto salir;

                        //Si se pulsa Sí, se indica que no quieren introducirse más alarmas
                        if(pr.horas_inicio==SI)
                        {
                           //Si era la primera alarma que se gestionaba, se interpreta como que
                           //no se quiere activar ninguna alarma
                           if(contador==0)
                           {
                              //Desactivamos las alarmas
                              PCF8583_write_byte(PCF8583_CTRL_STATUS_REG,
                                    PCF8583_START_COUNTING);
                              //Habilitamos interrupciones
                              //enable_interrupts(GLOBAL);
                              //Salimos
                              goto salir;
                           }
                           else
                              //Salimos simplemente del bucle
                              break;
                        }

                        //Movemos el cursor en la pantalla
                        lcd_gotoxy(5,2);

                        //Se pide al usuario el valor de los minutos de inicio
                        pr.minutos_inicio = buscar_numero();

                        //Si se pulsa NO, se sale
                        if(pr.minutos_inicio==NO)
                           goto salir;

                        //Si se pulsa SI, interrumpimos la introducción de alarmas
                        if(pr.minutos_inicio==SI)
                        {
                           //Si es el primer intervalo, interpretamos como que el usuario
                           //no quiere programar alarmas
                           if(contador==0)
                           {
                              //Desactivamos las alarmas
                              PCF8583_write_byte(PCF8583_CTRL_STATUS_REG,
                                    PCF8583_START_COUNTING);
                              //Habilitamos interrupciones
                              //enable_interrupts(GLOBAL);
                              goto salir;
                           }
                           else
                              //Salimos del bucle
                              break;
                        }

                        //********************************************************************************
                        //Los comentarios a partir de este punto y hasta la siguiente línea de asteriscos
                        //son equivalentes a los mostrados arriba pero esta vez con la hora de finalización

                        lcd_gotoxy(9,2);

                        pr.horas_fin = buscar_numero();

                        if(pr.horas_fin==NO)
                           goto salir;

                        if(pr.horas_fin==SI)
                        {
                           if(contador==0)
                           {
                              PCF8583_write_byte(PCF8583_CTRL_STATUS_REG,
                                    PCF8583_START_COUNTING);
                              //Habilitamos interrupciones
                              //enable_interrupts(GLOBAL);
                              goto salir;
                           }
                           else
                              break;
                        }


                        lcd_gotoxy(12,2);

                        pr.minutos_fin = buscar_numero();

                        if(pr.minutos_fin==NO)
                           goto salir;

                        if(pr.minutos_fin==SI)
                        {
                           if(contador==0)
                           {
                              PCF8583_write_byte(PCF8583_CTRL_STATUS_REG,
                                    PCF8583_START_COUNTING);
                              //Habilitamos interrupciones
                              //enable_interrupts(GLOBAL);
                              goto salir;
                           }
                           else
                              break;
                        }

                        //Ver comentarios arriba
                        //************************************************************

                        //Desactivamos el cursor de la pantalla
                        lcd_send_byte(LCD_ORDEN, LCD_NO_CURSOR);

                        //Introducimos cierto retardo para que el usuario pueda visualizar los valores introducidos en la pantalla
                        delay_ms(LCD_T_RETARDO);

                        //Comprobamos que los valores introducidos son correctos: horas menores que 24 y minutos menores que 60
                        //Además, comprobamos que la hora de finalización no vaya antes que la de inicio
                        if(pr.horas_inicio>23 || pr.horas_fin>23 || pr.minutos_inicio>59 || pr.minutos_fin>59
                              || comprobar_hora(pr.horas_fin,pr.minutos_fin,pr.horas_inicio,pr.minutos_inicio))
                        {
                           //Si se produce un error de este tipo, se indica
                           lcd_send_byte(LCD_ORDEN, LCD_CLEAR);
                           printf(lcd_putc, "   Valores\n   incorrectos");
                           delay_ms(LCD_T_RETARDO);
                           //Se repite la última iteración del bloque
                           contador--;
                           continue;
                        }

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
                        lcd_gotoxy(4,2);
                        lcd_send_byte(LCD_ORDEN, LCD_CURSOR); //Activamos el cursor

                        //Recogemos el número introducido por el usuario
                        pr.termostato = buscar_numero();

                        //Eliminamos el cursor
                        lcd_send_byte(LCD_ORDEN, LCD_NO_CURSOR);

                        //Retardamos para que el usuario pueda ver el valor que ha introducido
                        delay_ms(LCD_T_RETARDO);

                        //Si no se cumplen los límites de temperatura especificados para el termostato, no es válido el valor
                        if(pr.termostato>temp_max || pr.termostato<temp_min)
                        {
                           //Se muestra un mensaje de error
                           lcd_send_byte(LCD_ORDEN, LCD_CLEAR);
                           printf(lcd_putc, "   Valor\n   incorrecto");
                           delay_ms(LCD_T_RETARDO);
                           //Se repite la última iteración
                           contador--;
                           continue;
                        }

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

                     //Mostramos un mensaje
                     lcd_send_byte(LCD_ORDEN,LCD_CLEAR);
                     lcd_send_byte(LCD_ORDEN,LCD_NO_CURSOR);
                     printf(lcd_putc, "  Aplicando\n  cambios...");

                     //Habilitamos la alarma
                     PCF8583_write_byte(PCF8583_CTRL_STATUS_REG,
                                 PCF8583_ACTIVAR_ALARMA);
                     //Habilitamos interrupciones
                     //enable_interrupts(GLOBAL);

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
            case '4':
                     key = 'J'; //Carácter falso para permitir la entrada de datos
                     //Solicitamos la introducción de una clave
                     lcd_send_byte(LCD_ORDEN, LCD_CLEAR);
                     lcd_send_byte(LCD_ORDEN, LCD_CURSOR);
                     printf(lcd_putc, "  CLAVE:\n  ");

                     //Habilitamos de nuevo el temporizador para evitar
                     //que el menú esté mucho tiempo activo
                     contador_tiempo = 0;
                     enable_interrupts(INT_TIMER1);

                     //Introducimos primero los dos primero dígitos
                     contador = 0;
                     while(contador!=2)
                     {
                        key = get_key();
                        //Si se trata de un número, lo guardamos y mostramos un asterisco
                        //por tratarse de una clave
                        if(isdigit(key)){
                           //Reseteamos el contador de espera
                           contador_tiempo = 0;
                           lcd_putc('*');
                           keys[contador] = key;
                           key = 'J';
                           contador++;
                        }
                        //Si transcurre el tiempo máximo, salimos, anulando el flag
                        if(timer_int){
                           timer_int = FALSE;
                           goto salir;
                        }
                     }

                     //Convertimos los dígitos introducidos a un valor numérico
                     numero = atoi(keys);

                     //Repetimos la operación con los dos dígitos menos significativos
                     contador = 0;
                     while(contador!=2)
                     {
                        key = get_key();
                        if(isdigit(key)){
                           //Reseteamos el contador de espera
                           contador_tiempo = 0;
                           lcd_putc('*');
                           keys[contador] = key;
                           key = 'J';
                           contador++;
                        }
                        //Si transcurre el tiempo máximo, salimos, suprimiendo el flag
                        if(timer_int){
                           timer_int = FALSE;
                           goto salir;
                        }
                     }

                     //Deshabilitamos el control de la espera
                     disable_interrupts(INT_TIMER1);

                     //Construimos la clave a partir de las dos lecturas
                     numero = numero*100 + atoi(keys);

                     //Retardamos un poco para que el usuario pueda ver que ha introducido 4 dígitos
                     delay_ms(LCD_T_RETARDO);

                     if(numero == clave)
                     {
                        //Si la clave es correcta, entramos en el modo revisión
                        lcd_send_byte(LCD_ORDEN, LCD_CLEAR);
                        lcd_send_byte(LCD_ORDEN, LCD_NO_CURSOR);
                        printf(lcd_putc, "  Modo\n  revision");
                        //Representamos el contenido de los registros y activamos la interfaz RS-232
                        representar_registros();
                     }else
                     {
                        //Si la clave es incorrecta, lo indicamos y salimos
                        lcd_send_byte(LCD_ORDEN, LCD_CLEAR);
                        lcd_send_byte(LCD_ORDEN, LCD_NO_CURSOR);
                        printf(lcd_putc, "  Clave\n  incorrecta");
                        delay_ms(LCD_T_RETARDO);
                        goto salir;
                     }

                     break;


            //Si se pulsa NO, salimos del menú de interfaz con el usuario
            case 'N':
                     //Aquí confluyen todas las salidas de esta rutina
                     salir:
                     //Se muestra un mensaje de finalización
                     lcd_send_byte(LCD_ORDEN,LCD_CLEAR);
                     lcd_send_byte(LCD_ORDEN,LCD_NO_CURSOR);
                     printf(lcd_putc, "  Anulando\n  operacion...");
                     delay_ms(LCD_T_RETARDO);
                     break;

         }

         //Volvemos a habilitar las interrupciones
         enable_interrupts(GLOBAL);
         

         //Se desactiva el flag de indicación de la interrupción
         led_int = FALSE;
      }

      //Comenzamos el bloque de comprobación de parámetros y activación de los procesos necesarios.

      //Comparamos primero la temperatura del agua en la caldera. Esta comprobación se
      //realiza siempre, independientemente de que el sistema esté o no encendido.
      if(C1OUT)
      {
         //Entramos aquí si la temperatura de la caldera es menor que la necesaria
         if(temperatura_caldera_superada)
         {
            //Si la temperatura de la caldera estaba superada la última vez, tenemos en cuenta cierta histéresis
            //de los valores.
            if(!comp_caldera())
            {
               //Si la temperatura de la caldera es menor que la necesaria, encendemos la caldera.
               temperatura_caldera_superada = FALSE;
               encender_caldera();
            }

         }
         else if(!caldera_encendida)
         {
            //Si no estaba encendida, la encendemos
            encender_caldera();
         }
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

      //Leemos el código de estado de los sensores
      valor = comprobar_temperatura();

      //Comprobamos que el sistema no lleve encendido más de 4 horas, ya que en ese caso hay riesgo de avería
      //(en realidad esta limitación deriva de un problema propio de representación en memoria).
      //Ya leimos el tiempo en comprobar_temperatura(), así que no necesitamos leerlo de nuevo.
      if(sistema_encendido && diferencia_tiempo(hora_encendido, minutos_encendido, tiempo.hours, tiempo.minutes)>240)
      {
         //Si la diferencia de tiempo es mayor que 4 horas, lo indicamos y apagamos los motores y el sistema
         lcd_send_byte(LCD_ORDEN,LCD_CLEAR);
         printf(lcd_putc, "Demasiado tiempo\nencendido!");
         delay_ms(2*LCD_T_RETARDO);
         lcd_send_byte(LCD_ORDEN,LCD_CLEAR);
         printf(lcd_putc, "  Apagando\n  sistema...");
         apagar_sistema();

         motor1 = FALSE;
         motor2 = FALSE;

         //Si estaba encendido por alguna alarma, recuperamos el termostato anterior.
         if(encendido_por_alarma)
         {
            termostato1 = termostato2 = termostato_provisional;
            encendido_por_alarma = FALSE;
         }

         goto inicio;

      }

      //Comprobamos ahora la temperatura de la vivienda. Esto se realiza sólo si el sistema está encendido
      if(valor && sistema_encendido && temperatura_caldera_superada)
      {
         //Si la temperatura es menor que la necesaria, el sistema está encendido y la temperatura del
         //agua en la caldera es la adecuada, encendemos el motor de circulación del agua necesario
         if(valor==1)
         {
            motor1 = TRUE;
            motor2 = FALSE;
         }
         if(valor==2)
         {
            motor1 = FALSE;
            motor2 = TRUE;
         }
         if(valor==3)
         {
            motor1 = TRUE;
            motor2 = TRUE;
         }

      }
      else
      {
        //Si no se cumplen las condiciones, apagamos los motores si están encendidos
        motor1 = FALSE;
        motor2 = FALSE;
      }

      //Activamos el perro guardián y las interrupciones
      setup_wdt(WDT_ON);
      enable_interrupts(GLOBAL);
      //Ponemos a "dormir" al microcontrolador para que el gasto de energía sea menor
      //Esto lo hacemos sólo si no se han levantado los flags de interrupción en estos segundos,
      //ya que esto provocaría que se aplazase la gestión de la interrupción hasta que saltase el
      //perro guardián.
      if(!alarma_int && !led_int)
         sleep();

   }

}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//****************************************************************************
//    Descripción de los métodos utilizados ordenados por orden alfabético
//****************************************************************************
//Puede verse una explicación general de su función al principio del programa

void apagar_caldera()
{
   int hora, minutos, segundos; //Variables para el tiempo

   //Leemos el tiempo
   PCF8583_read_datetime(&tiempo);
   //Habilitamos interrupciones
   //enable_interrupts(GLOBAL);

   //Copiamos en las variables
   hora = tiempo.hours;
   minutos = tiempo.minutes;
   segundos = tiempo.seconds;

   //Sumamos al tiempo total de encendido de la caldera el tiempo que ha estado encendida esta vez
   t_total_caldera = t_total_caldera + diferencia_tiempo_sec(hora_caldera, minutos_caldera, segundos_caldera, hora, minutos, segundos);

   //Apagamos la caldera
   caldera_encendida = FALSE;
}

//-----------------------------------------------------------------------------------------------------------------------------------------

void apagar_sistema()
{
   long minutos_trans;  //Minutos transcurridos desde el encendido del sistema
   int t_entera, t_decimal; //Temperatura del hogar en ese momento (parte entera y decimal)
   int data[8];  //Vector para almacenar los datos a guardar

   //Apagamos el sistema
   sistema_encendido = FALSE;

   //Guardamos todos los datos en memoria. Esto llevará tiempo, así que apagamos también los motores
   //para no esperar a la rutina de comprobación de la temperatura
   motor1 = FALSE;
   motor2 = FALSE;

   //Si llegamos al límite de la memoria, se resetea el indicador
   if((eeprom_registros + num_registros*2*8)>=0x1000)
      num_registros = 0;

   //Leemos el tiempo actual
   PCF8583_read_datetime(&tiempo);
   //Habilitamos interrupciones (se desactivan en el método anterior)
   //enable_interrupts(GLOBAL);

   //Almacenamos en el vector los valores del tiempo
   data[0] = tiempo.day;                  //Día de desconexión
   data[1] = tiempo.month;                //Mes de desconexión
   data[2] = tiempo.hours;                //Hora de desconexión
   data[3] = tiempo.minutes;              //Minutos de desconexión

   //Leemos la temperatura de la zona 1 en ese momento
   temperatura1 = ds1820_read(dir1);

   //Descomponemos la temperatura en su parte entera y parte decimal para almacenarla
   t_entera = (int)temperatura1;
   t_decimal = (int)((temperatura1-t_entera)*100);

   //Almacenamos los valores
   data[4] = t_entera;                    //Valor entero de última temperatura medida en la zona 1
   data[5] = t_decimal;                   //Valor decimal de última temperatura medida en la zona 1

   //Repetimos el proceso con la temperatura de la zona 2
   temperatura2 = ds1820_read(dir2);
   t_entera = (int)temperatura2;
   t_decimal = (int)((temperatura2-t_entera)*100);

   //Almacenamos
   data[6] = t_entera;                    //Valor entero de última temperatura medida en la zona 2
   data[7] = t_decimal;                   //Valor decimal de última temperatura medida en la zona 2

   //Grabamos los datos en memoria
   graba_ee((unsigned long)eeprom_registros + num_registros*(unsigned long)2*(unsigned long)8, 8, data);


   //Calculamos el año en el que nos encontramos
   if(anno_actual_0_to_3 != tiempo.year)
   {
      //Si el año que teníamos almacenado no es igual que el que marca el reloj,
      //debemos de actualizar las variables correspondientes.
      anno_actual++;
      anno_actual_0_to_3++;

      //Guardamos los valores en el vector data
      data[0] = anno_actual;
      data[1] = anno_actual_0_to_3;

      //Guardamos los valores en memoria
      graba_ee(eeprom_anno_actual, 2, data);
   }

   //Guardamos el año en el que nos encontramos
   data[0] = anno_actual;

   //Calculamos los minutos transcurridos desde el encendido como una diferencia de tiempos
   minutos_trans = diferencia_tiempo(hora_encendido, minutos_encendido, tiempo.hours, tiempo.minutes);

   //Si la caldera estaba encendida, sumamos el último intervalo de encendido al tiempo total
   if(caldera_encendida)
      t_total_caldera = t_total_caldera + diferencia_tiempo_sec(hora_caldera, minutos_caldera, segundos_caldera, tiempo.hours, tiempo.minutes, tiempo.seconds);

   //Guardamos los dos tiempos calculados
   data[1] = (unsigned int)minutos_trans;          //Minutos transcurridos desde el encendido
   data[2] = (unsigned int)(t_total_caldera/60);   //Tiempo total que ha estado encendida la caldera
                                                   //en este intervalo (en minutos)

   //Guardamos también el valor del termostato teniendo en cuenta la histéresis
   if(temp_habitacion_superada1)
      data[3]=(int)(termostato1+histeresis_vivienda);
   else
      data[3]=(int)termostato1;

   //Grabamos esta segunda tanda de datos
   graba_ee((unsigned long)eeprom_registros + num_registros*2*8 + 8, 4, data);

   //Aumentamos en uno el número de registros
   num_registros++;

   //Guardamos el número de registros en memoria (descomponiendo antes en dos int)
   data[0] = (int)(num_registros/256);
   data[1] = (int)(num_registros - data[0]*256);

   graba_ee(eeprom_num_registros, 2, data);
}

//-----------------------------------------------------------------------------------------------------------------------------------------

int buscar_numero(){
   //Variables auxiliares
   int contador = 0;
   int numero;

   //Carácter falso para evitar confusiones
   key = 'J';

   //Habilitamos el temporizador para evitar que el usuario mantenga
   //mucho tiempo el menú activo
   contador_tiempo = 0;
   enable_interrupts(INT_TIMER1);

   //Se continua la búsqueda hasta que no se pulse SI o NO o hasta
   //que no se introduzca un número de 2 cifras.
   while(key != 'N' && key!='S' && contador!=2)
   {
      key = get_key();
      //Si la tecla corresponde a un dígito, la imprimimos por pantalla
      //y pasamos a la siguiente
      if(isdigit(key)){
         //Reseteamos el contador de tiempo
         contador_tiempo = 0;
         lcd_putc(key);
         keys[contador] = key;
         key = 'J';
         contador++;
      }
      //Si salta el tiempo previsto, devuelve el valor de anulación de la operación
      if(timer_int){
         key = 'N';
         timer_int = FALSE; //Desactivamos el flag
      }
   }

   //Volvemos a deshabilitar la interrupción del timer
   disable_interrupts(INT_TIMER1);

   //Si la tecla es NO, devolvemos el código correspondiente
   if(key == 'N')
      return NO;

   //Procedemos de la misma manera si es SÍ
   if(key == 'S')
      return SI;

   //Transformamos los caracteres a entero
   numero = atoi(keys);

   //Devolvemos el resultado
   return numero;
}

//-----------------------------------------------------------------------------------------------------------------------------------------

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
      return NO;
   //Imprimimos la primera cifra
   printf("%c", c[0]);

   //Leemos la segunda cifra del número, procediendo de la misma manera
   while(!isdigit(c[1]) && c[1]!='\b')
      c[1]=getch();
   if(c[1]=='\b')
      return NO;
   printf("%c", c[1]);

   //Transformamos a entero la cadena y la devolvemos
   return atoi(c);
}

//-----------------------------------------------------------------------------------------------------------------------------------------

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
   if(temp > termo-histeresis_caldera)
      return TRUE;
   else
      return FALSE;
}

//-----------------------------------------------------------------------------------------------------------------------------------------

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

//-----------------------------------------------------------------------------------------------------------------------------------------

int comprobar_temperatura()
{
   //Flags auxiliares
   int1 flag1, flag2;

   //Leemos la temperatura
   temperatura1 = ds1820_read(dir1);
   temperatura2 = ds1820_read(dir2);

   //Mostramos la información en la pantalla lcd
   mostrar_temperatura();

   //************************************
   //NOTA: El termostato no lo hemos implementado con la función ALARM SEARCH del ds1820
   //por dos razones: por necesitar un algoritmo bastante complejo en el caso de dos dispositivos
   //y por que se ha llegado a la conclusión de que consume más memoria y tiempo, ya que
   //de todas formas tenemos que leer las temperaturas para mostrarlas por pantalla. Si se
   //quiere ver un ejemplo de utilización de este modo de termostato del ds1820, en el prototipo
   //construido para probar en la EasyPic3 se ha implementado con un único sensor.
   //************************************

   //Nos centramos primero en el sensor 1
   if(temperatura1 < termostato1)
   {
      //Si la temperatura es menor que la del termostato, devolvemos TRUE
      if(temp_habitacion_superada1)
      {
         //Si la temperatura antes era mayor, indicamos el cambio en el flag
         temp_habitacion_superada1 = FALSE;
         //Tenemos en cuenta la histéresis para el nuevo valor del termostato
         termostato1 = termostato1 + histeresis_vivienda;
      }
      flag1 = TRUE;
   }
   else
   {
      //Si la temperatura es mayor que la del termostato, devolvemos FALSE
      if(!temp_habitacion_superada1)
      {
         //Si antes era menor, cambiamos el flag
         temp_habitacion_superada1 = TRUE;
         //Guardamos el nuevo valor del termostato teniendo en cuenta la histéresis
         termostato1 = termostato1 - histeresis_vivienda;
      }
      flag1 = FALSE;
   }

   //Repetimos las mismas operaciones para el sensor 2
   if(temperatura2 < termostato2)
   {
      //Si la temperatura es menor que la del termostato, devolvemos TRUE
      if(temp_habitacion_superada2)
      {
         //Si la temperatura antes era mayor, indicamos el cambio en el flag
         temp_habitacion_superada2 = FALSE;
         //Tenemos en cuenta la histéresis para el nuevo valor del termostato
         termostato2 = termostato2 + histeresis_vivienda;
      }
      flag2 = TRUE;
   }
   else
   {
      //Si la temperatura es mayor que la del termostato, devolvemos FALSE
      if(!temp_habitacion_superada2)
      {
         //Si antes era menor, cambiamos el flag
         temp_habitacion_superada2 = TRUE;
         //Guardamos el nuevo valor del termostato teniendo en cuenta la histéresis
         termostato2 = termostato2 - histeresis_vivienda;
      }
      flag2 = FALSE;
   }

   //Devolvemos el código correspondiente según se explica en la definición de la función
   if(!flag1 && !flag2)
      return 0;
   if(flag1 && !flag2)
      return 1;
   if(!flag1 && flag2)
      return 2;
   if(flag1 && flag2)
      return 3;
}

//-----------------------------------------------------------------------------------------------------------------------------------------

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

//-----------------------------------------------------------------------------------------------------------------------------------------

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

//-----------------------------------------------------------------------------------------------------------------------------------------

void encender_caldera()
{
   //Leemos el tiempo actual
   PCF8583_read_datetime(&tiempo);
   //Habilitamos interrupciones (se desactivan en el método anterior)
   //enable_interrupts(GLOBAL);

   //Guardamos los datos en las variables correspondientes
   hora_caldera = tiempo.hours;
   minutos_caldera = tiempo.minutes;
   segundos_caldera = tiempo.seconds;

   //Encendemos la caldera
   caldera_encendida = TRUE;
}

//-----------------------------------------------------------------------------------------------------------------------------------------

void encender_sistema()
{
   //Leemos el tiempo actual
   PCF8583_read_datetime(&tiempo);
   //Habilitamos interrupciones (se desactivan en el método anterior)
   //enable_interrupts(GLOBAL);

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

//-----------------------------------------------------------------------------------------------------------------------------------------

void grabar_programaciones()
{
   int datos[5];   //Vector para contener los datos a grabar
   programacion pr; //Variable temporal de programacion
   int contador;   //Contador para el bucle

   //Recorremos todo el vector de programaciones (hasta el número de programaciones que tengamos)
   for(contador=0; contador<num_intervalos; contador++)
   {
      //Seleccionamos la programación
      pr = programaciones[contador];

      //Almacenamos los datos en el vector
      datos[0] = pr.horas_inicio;
      datos[1] = pr.minutos_inicio;
      datos[2] = pr.horas_fin;
      datos[3] = pr.minutos_fin;
      datos[4] = pr.termostato;

      //Guardamos los datos en la posición de memoria correspondiente de la EEPROM
      graba_ee(eeprom_programaciones + (contador+1)*8, 5, datos);
   }

   //Guardamos en la EEPROM en el número de intervalos
   graba_ee(eeprom_num_intervalos, 1, &num_intervalos);

   //He optado por escribir de programación en programación porque a pesar de que
   //en el datasheet aseguran que la memoria tiene una paginación de 32 bytes en la
   //simulación parece tener menos, así que de este modo nos evitamos problemas
}

//-----------------------------------------------------------------------------------------------------------------------------------------

void inicializacion()
{
   //NOTA: todos los pequeños retardos introducidos en esta rutina sirven para que el técnico
   //no se sienta atosigado por los continuos mensajes que le aparecen solicitándole información.


   //Variables para introducir los datos de fecha y hora
   int weekday, dia, mes, anno, horas, minutos;
   //Variable para escritura en memoria
   int data[8];
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
   if(dia==NO)
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
   if(mes==NO)
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
   if(anno==NO)
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
   if(horas==NO)
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
   if(minutos==NO)
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
   termostato1 = termostato2 = (float)20;
   num_intervalos = 0;
   num_registros = 0;
   anno_actual = anno;

   //Escribimos los datos en la EEPROM, almacenando antes en un vector para
   //agilizar la escritura por ráfaga
   data[0] = (int)termostato1;
   data[1] = num_intervalos;
   data[2] = (int)(num_registros/256);
   data[3] = (int)(num_registros - data[0]*256);
   data[4] = anno_actual;
   data[5] = anno_actual_0_to_3;

   graba_ee(eeprom_termostato, 6, data);

   //Mostramos un mensaje de inicio del sistema
   printf("\rIniciando sistema...\r");

}

//-----------------------------------------------------------------------------------------------------------------------------------------

void leer_programaciones(){
   int datos[5];    //Vector para almacenar los datos recuperados
   programacion pr;  //Variable temporal para guardar la programación
   int contador;    //Contador del bucle

   //Se supone que antes ha sido recuperado el número de intervalos
   //Recorremos todas las programaciones guardadas
   for(contador=0; contador<num_intervalos; contador++)
   {
      //Leemos de memoria
      lee_ee(eeprom_programaciones + (contador+1)*8, 5, datos);

      //Guardamos los datos en la programación
      pr.horas_inicio   = datos[0];
      pr.minutos_inicio = datos[1];
      pr.horas_fin      = datos[2];
      pr.minutos_fin    = datos[3];
      pr.termostato     = datos[4];

      //Guardamos la programación en el vector correspondiente
      programaciones[contador] = pr;

   }

}

//-----------------------------------------------------------------------------------------------------------------------------------------

void mostrar_temperatura()
{
   //Leemos el tiempo y lo guardamos en la variable tiempo
   PCF8583_read_datetime(&tiempo);
   //Activamos las interrupciones, que se desactivan dentro del método
   //enable_interrupts(GLOBAL);

   //Mostramos la temperatura registrada
   lcd_send_byte(LCD_ORDEN, LCD_CLEAR);
   printf(lcd_putc, "%3.1f%cC  %3.1f%cC\nHORA - ", temperatura1, 223, temperatura2, 223);

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

//-----------------------------------------------------------------------------------------------------------------------------------------

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

//-----------------------------------------------------------------------------------------------------------------------------------------

void programar_proxima_alarma()
{
   //Variables auxiliares
   int hora, minutos;

   //Recuperamos los datos de inicio de la alarma actual
   prg=programaciones[posicion_alarmas];
   hora = prg.horas_inicio;
   minutos = prg.minutos_inicio;

   //Establecemos la alarma a ese tiempo
   PCF8583_establecer_alarma(hora, minutos, 0);
   //Habilitamos interrupciones (se desactivan en el método anterior)
   //enable_interrupts(GLOBAL);
}

//-----------------------------------------------------------------------------------------------------------------------------------------

void representar_registros()
{
   //Contadores y variables auxiliares
   int contador, contador2;
   unsigned int data[8], data2[4];

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
   int dia[3];               //Día con el que estamos trabajando (día/mes/año)
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
   dia[0] = dia[1] = dia[2] = 0;
   num_dias = 0;
   num_annos = 0;

   media_dia_caldera = 0;
   valor = 0;
   valor_total = 0;
   valor_total_caldera = 0;

   //Imprimimos el encabezamiento
   printf("\rREGISTRO DE EVENTOS\r");
   printf("*******************\r");
   printf("Momento de apagado     Temperatura1       Temperatura2         Termostato        Minutos encendido       Minutos caldera\r");
   printf("===========================================================================================================================\r");

   //Recorremos todos los registros almacenados
   for(contador=0; contador<num_registros; contador++)
   {
      //Comenzamos leyendo los datos de memoria
      lee_ee((unsigned long)eeprom_registros + (unsigned long)contador*(unsigned long)2*(unsigned long)8, 8, data);
      lee_ee((unsigned long)eeprom_registros + (unsigned long)contador*(unsigned long)2*(unsigned long)8 + (unsigned long)8, 4, data2);

      //Si cambiamos de día debemos gestionar los datos del nuevo día y almacenar los del anterior
      if(dia[0]!=data[0] || dia[1]!=data[1] || dia[2]!=data2[0])
      {
         //Lógicamente, no tenemos en cuenta para la media los días en los que no se enciende:
         //días de verano, calurosos, etc. Esta información puede visualizarse en la media
         //por meses.
         //Guardamos los datos del día anterior si el día no es el primero
         if(dia[0]!=0)
         {
            //En la media de tiempo por día, acumulamos el tiempo que ha estado encendido el último día
            valor = valor + (float)tiempo_x_dia;
            //Guardamos también este valor en el registro del año
            media_dia_mes[dia[1]-1] = valor;
            //Realizamos la misma operación con la media de encendido de la caldera
            media_dia_caldera = media_dia_caldera + (float)tiempo_x_dia_caldera;
            media_dia_mes_caldera[dia[1]-1] = media_dia_caldera;
         }
         //La sumatoria podría alcanzar un valor muy alto. El sistema no está preparado para estas eventualidades
         //y debería ser reseteado cada cierto tiempo. Si el sistema va a utilizarse mucho (zonas muy frías o mal
         //aisladas), el usuario debería informar al fabricante para aumentar la memoria de almacenamiento y el
         //tamaño de las variables

         //Si cambiamos de año, habrá que guardar los datos anuales
         if(dia[2]!=data2[0])
         {
            //Comprobamos que no nos encontrabamos en el año introducido por defecto
            if(dia[2]!=0)
            {
               //En ese caso, guardamos los datos de todo el año
               for(contador2=0; contador2<12; contador2++)
               {
                  media_dia_mes[contador2] = media_dia_mes[contador2]/(float)dias_meses[contador2];
                  media_dia_mes_caldera[contador2] = media_dia_mes_caldera[contador2]/(float)dias_meses[contador2];
                  media_annos[num_annos][contador2] = media_dia_mes[contador2];
                  media_annos_caldera[num_annos][contador2] = media_dia_mes_caldera[contador2];
                  //Volvemos a inicializar los contenedores de información anual
                  media_dia_mes[contador2]=0;
                  media_dia_mes_caldera[contador2]=0;
               }

               //Guardamos el año del que se trata
               annos[num_annos]=dia[2];

               //Aumentamos en 1 el número de años
               num_annos++;
            }

            //COMPROBAR BISIESTO
            //En realidad, un año será bisiesto si es divisible por 4 y no lo es por 100,
            //excepto si este último lo es por 400. Sin embargo, dado que trabajamos con las
            //dos últimas cifras y que el próximo año divisible por 4 que no lo es por 100
            //es el 2100, mejor nos limitaremos a la divisibilidad por 4.
            //Dependiendo si el año es bisiesto o no, le damos a febrero 29 o 28 días.
            if(data2[0]%4 == 0)
               dias_meses[1] = 29;
            else
               dias_meses[1] = 28;
         }

         //Modificamos la información del día
         dia[0] = data[0];
         dia[1] = data[1];
         dia[2] = data2[0];

         //Aumentamos en 1 el número de días
         num_dias++;

         //Reseteamos las variables acumulativas de tiempo por día
         tiempo_x_dia = 0;
         tiempo_x_dia_caldera = 0;
      }

      //Le sumamos al tiempo de encendido de sistema y caldera el correspondiente de este registro
      tiempo_x_dia = tiempo_x_dia + data2[1];
      tiempo_x_dia_caldera = tiempo_x_dia_caldera + data2[2];
      //También incrementamos los tiempos totales
      valor_total = valor_total + data2[1];
      valor_total_caldera = valor_total_caldera + data2[2];

      //Representamos todos los datos, teniendo en cuenta algunos requisitos representativos
      //para que los datos queden alineados (número menores que 10, que 100, etc).
      if(data[0]<10)
         printf(" ");

      printf("%u/", data[0]);

      if(data[1]<10)
         printf("0");

      printf("%u/", data[1]);

      if(data2[0]<10)
         printf("0");

      printf("%u  ", data2[0]);

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

      printf("%u%cC             ", data[5], 223);

      if(data[6]<10)
         printf(" ");

      printf("%u.", data[6]);

      if(data[7]<10)
         printf("0");

      printf("%u%cC              %u%cC                  ", data[7], 223, data2[3], 223);

      if(data2[1]<100)
         printf(" ");
      if(data2[1]<10)
         printf(" ");

      printf("%u                    ", data2[1]);

      if(data2[2]<100)
         printf(" ");
      if(data2[2]<10)
         printf(" ");

      printf("%u\r", data2[2]);

   }

   //Debemos realizar la operación de guardado para el último día. El proceso es el mismo
   //que hemos seguido arriba.
   if(dia[0]!=0)
   {
      valor = valor + (float)tiempo_x_dia;
      media_dia_mes[dia[1]-1] = valor;
      media_dia_caldera = media_dia_caldera + (float)tiempo_x_dia_caldera;
      media_dia_mes_caldera[dia[1]-1] = media_dia_caldera;
   }

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

   //Calculamos la media como el cociente entre los valore de tiempo calculados y el número de
   //días en los que se ha encendido la caldera
   valor = valor/(float)num_dias;
   media_dia_caldera = media_dia_caldera/(float)num_dias;

   repetir:
   //Menú para mostrar los datos
   printf("\r===========================================================================================================================\r");
   printf("¿Que operacion desea realizar?\r");
   printf("0. Salir\r");
   printf("1. Extraer tiempos medios por dia\r");
   printf("2. Extraer tiempos totales\r");
   printf("3. Extraer datos mensuales\r");
   printf("===========================================================================================================================\r");

   //Esperamos la introducción de una de las opciones por parte del técnico
   while(key!='1' && key!='2' && key!='3' && key!='0'){
      key=getch();
   }

   switch(key)
   {
      //En el caso 0, se sale del sistema
      case '0':
               //Se indica la salida mediante un mensaje
               printf("\rSaliendo...\r");
               break;

      //En el caso 1, se muestran los valores medios
      case '1':
               key = 'J';  //Utilizamos este carácter para evitar la elección errónea de la misma acción
               printf("\rTiempo medio de encendido del sistema por dia: %3.2f minutos\r\r", valor);
               printf("Tiempo medio de encendido de la caldera por dia: %3.2f minutos\r\r", media_dia_caldera);
               goto repetir;
               break;

      //En el caso 2, se muestran los valores totales
      case '2':
               key = 'J'; //Ídem
               printf("\rTiempo total de encendido del sistema: %lu minutos\r\r", valor_total);
               printf("Tiempo total de encendido de la caldera: %lu minutos\r\r", valor_total_caldera);
               goto repetir;
               break;

      //En el caso 3, se muestran todos los valores medios por meses y años
      case '3':
               key = 'J'; //Ídem
               //Se representan todos los años de los que se tiene registro
               for(contador=0; contador<num_annos; contador++)
               {
                  //Imprimimos el año en el que nos encontramos
                  printf("\r20");
                  if(annos[contador]<10)
                     printf("0");
                  printf("%u\r", annos[contador]);
                  printf("--------------------------------------------------------------------------------------------------------------------\r");
                  printf("              Tiempo medio de encendido del sistema (min/dia)      Tiempo medio de encendido de la caldera (min/dia)\r");
                  printf("ENERO                              %3.2f                                                     %3.2f\r", media_annos[contador][0], media_annos_caldera[contador][0]);
                  printf("FEBRERO                            %3.2f                                                     %3.2f\r", media_annos[contador][1], media_annos_caldera[contador][1]);
                  printf("MARZO                              %3.2f                                                     %3.2f\r", media_annos[contador][2], media_annos_caldera[contador][2]);
                  printf("ABRIL                              %3.2f                                                     %3.2f\r", media_annos[contador][3], media_annos_caldera[contador][3]);
                  printf("MAYO                               %3.2f                                                     %3.2f\r", media_annos[contador][4], media_annos_caldera[contador][4]);
                  printf("JUNIO                              %3.2f                                                     %3.2f\r", media_annos[contador][5], media_annos_caldera[contador][5]);
                  printf("JULIO                              %3.2f                                                     %3.2f\r", media_annos[contador][6], media_annos_caldera[contador][6]);
                  printf("AGOSTO                             %3.2f                                                     %3.2f\r", media_annos[contador][7], media_annos_caldera[contador][7]);
                  printf("SEPTIEMBRE                         %3.2f                                                     %3.2f\r", media_annos[contador][8], media_annos_caldera[contador][8]);
                  printf("OCTUBRE                            %3.2f                                                     %3.2f\r", media_annos[contador][9], media_annos_caldera[contador][9]);
                  printf("NOVIEMBRE                          %3.2f                                                     %3.2f\r", media_annos[contador][10], media_annos_caldera[contador][10]);
                  printf("DICIEMBRE                          %3.2f                                                     %3.2f\r", media_annos[contador][11], media_annos_caldera[contador][11]);
                  printf("--------------------------------------------------------------------------------------------------------------------\r");
               }

               goto repetir;
               break;

      //En los tres casos, vuelve a mostrarse el menú tras imprimir las estadísticas
   }
}

//-----------------------------------------------------------------------------------------------------------------------------------------

void seleccionar_alarma()
{
   //Contador y variables auxiliares
   int contador;
   int hora, minutos, hora_p, min_p;

   //Ponemos la posición en el vector de alarmas incialmente a 0
   posicion_alarmas = 0;

   //Leemos el tiempo actual
   PCF8583_read_datetime(&tiempo);
   //Habilitamos interrupciones (se desactivan en el método anterior)
   //enable_interrupts(GLOBAL);
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
