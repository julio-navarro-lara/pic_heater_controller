//******************************************
//Archivo de cabecera del Proyecto 1
//******************************************
//Author: Julio Navarro Lara         2010

//Definimos los puertos del microcontrolador
#byte port_a = 0xF80
#byte port_b = 0xF81
#byte port_c = 0xF82
#byte port_d = 0xF83
#byte port_e = 0xF84

#bit sistema_encendido = 0xF83.2    //Pin que indica el encendido del sistema con un valor alto
#bit motor = 0xF83.1                //Pin que indica el encendido del motor de circulación del agua con un valor alto
#bit caldera_encendida = 0xF83.0    //Pin que indica el encendido de la caldera con un valor alto
#bit menos =    0xF82.1             //Pines para gestionar la entrada de los pulsadores
#bit mas   =    0xF82.0             //''
#bit si    =    0xF82.2             //''
#bit no    =    0xF82.5             //''

//Código para definir el botón NO
#define NOCODE          100

//Parámetros de la pantalla LCD
#define LCD_ORDEN       0    //Código para enviar un orden
#define LCD_DATO        1    //Código para enviar un dato

#define LCD_CLEAR       0x01  //Orden para borrar la pantalla
#define LCD_NO_CURSOR   0x0C  //Orden para eliminar el cursor de la pantalla
#define LCD_CURSOR      0x0E  //Orden para visualizar el cursor en la pantalla
#define LCD_PARPADEAR   0x0D  //Orden para que parpadee el cursor

//Tiempo que se mantienen los mensajes temporales en pantalla (ms)
//En ocasiones este retardo se introduce simplemente para que el usuario o el técnico visualicen los propios
//valores que han introducido por pantalla sin ser atosigados por la siguiente operación.
#define LCD_T_RETARDO   500

//Parámetros del conversor AD
#define termostato_caldera   3   //Entrada AD que se refiere a la temperatura de referencia de la caldera
#define temperatura_caldera  2   //Entrada AD que se refiere a la temperatura del agua en la caldera
#define histeresis_caldera   5   //Diferencia de histéresis para el agua de la caldera
//Para probar en el PROTEUS, poner una histeresis de 10 ºC (los potenciómetros simulados no tienen más sensibilidad)
#define t_max_caldera        100  //Temperatura máxima a la que puede regularse la caldera
#define t_min_caldera        0  //Temperatura mínima a la que puede regularse la caldera
#define AD_num_valores       255 //Número de valores de escala del conversor (sensibilidad de los potenciómetros)

//Parámetros de control de temperatura
//Datos suministrados por el aparato de aire acondicionado (con bomba de calor) de mi casa
#define temp_max          32  //Temperatura máxima de regulación del termostato
#define temp_min          16  //Temperatura mínima de regulación del termostato

//Direcciones para la EEPROM
#define eeprom_termostato        0x00  //Valor del termostato
#define eeprom_num_intervalos    0x01  //Número de intervalos de programación de los que disponemos
#define eeprom_num_registros     0x02  //Número de registros de desconexión de los que se disponen
#define eeprom_anno_actual       0x03  //Año en el que nos encontramos
#define eeprom_anno_0_to_3       0x04  //Año en el que nos encontramos medido de 0 a 3 (llamado 1_to_3 por razones "históricas")
#define eeprom_programaciones    0x05  //Posición a partir de la cual se almacenan las programaciones de alarmas
#define eeprom_registros         0x14  //Posición a partir de la cual se almacenan los registros
                                       //En este caso, por usar la memoria interna del microprocesador, caben sólo 26 registros
