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

#bit sistema_encendido = 0xF83.5    //Pin que indica el encendido del sistema con un valor alto
#bit motor = 0xF83.6                //Pin que indica el encendido del motor de circulaci�n del agua con un valor alto
#bit caldera_encendida = 0xF83.7    //Pin que indica el encendido de la caldera con un valor alto
#bit teclado1 = 0xF81.4             //Pines para gestionar la entrada del teclado
#bit teclado2 = 0xF81.5             //''
#bit teclado3 = 0xF81.6             //''
#bit teclado4 = 0xF81.7             //''
#bit lcd_rs   = 0xF81.2             //Pin que controla la entrada RS de la pantalla LCD

//Par�metros de la pantalla LCD
#define LCD_ORDEN       0    //C�digo para enviar un orden
#define LCD_DATO        1    //C�digo para enviar un dato

#define LCD_CLEAR       0x01  //Orden para borrar la pantalla
#define LCD_NO_CURSOR   0x0C  //Orden para eliminar el cursor de la pantalla
#define LCD_CURSOR      0x0E  //Orden para visualizar el cursor en la pantalla
#define LCD_APAGAR      0x08  //Orden para apagar la pantalla

//Tiempo que se mantienen los mensajes temporales en pantalla (ms)
#define LCD_T_RETARDO   500

//Clave de acceso
#define clave           2401


//Par�metros del teclado
#define NUM_COLUMNAS 3     //N�mero de columnas del teclado matricial
#define NUM_FILAS    4     //N�mero de filas del teclado matricial
#define NO           100  //Codificaci�n del NO en el m�todo buscar_numero()
#define SI           101  //Codificaci�n del SI en el m�todo buscar_numero()

//Par�metros del conversor AD
#define termostato_caldera   3   //Entrada AD que se refiere a la temperatura de referencia de la caldera
#define temperatura_caldera  0   //Entrada AD que se refiere a la temperatura del agua en la caldera
#define histeresis_caldera   5   //Diferencia de hist�resis para el agua de la caldera
#define t_max_caldera        70  //Temperatura m�xima a la que puede regularse la caldera
#define t_min_caldera        25  //Temperatura m�nima a la que puede regularse la caldera
#define AD_num_valores       255 //N�mero de valores de escala del conversor (sensibilidad de los potenci�metros)

//Par�metros de control de temperatura
//Datos suministrados por el aparato de aire acondicionado (con bomba de calor) de mi casa
#define temp_max          32  //Temperatura m�xima de regulaci�n del termostato
#define temp_min          16  //Temperatura m�nima de regulaci�n del termostato


//Direcciones para la EEPROM
#define eeprom_termostato        0x10  //Valor del termostato
#define eeprom_num_intervalos    0x11  //N�mero de intervalos de programaci�n de los que disponemos
#define eeprom_num_registros     0x12  //N�mero de registros de desconexi�n de los que se disponen
#define eeprom_anno_actual       0x14  //A�o en el que nos encontramos
#define eeprom_anno_1_to_3       0x15  //A�o en el que nos encontramos medido de 0 a 3 (llamado 1_to_3 por razones "hist�ricas")
#define eeprom_programaciones    0x18  //Posici�n a partir de la cual se almacenan las programaciones de alarmas
#define eeprom_registros         0x50  //Posici�n a partir de la cual se almacenan los registros
