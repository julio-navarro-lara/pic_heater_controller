//******************************************
//Archivo de cabecera del Proyecto 1
//******************************************
//Author: Julio Navarro Lara         2010

#bit sistema_encendido = 0xF08.2    //Pin que indica el encendido del sistema con un valor alto
#bit motor = 0xF08.1                //Pin que indica el encendido del motor de circulaci�n del agua con un valor alto
#bit caldera_encendida = 0xF08.0    //Pin que indica el encendido de la caldera con un valor alto
#bit menos =    0xF07.1             //Pines para gestionar la entrada de los pulsadores
#bit mas   =    0xF07.0             //''
#bit si    =    0xF07.2             //''
#bit no    =    0xF07.5             //''

//Par�metros de la pantalla LCD
#define LCD_ORDEN       0    //C�digo para enviar un orden
#define LCD_DATO        1    //C�digo para enviar un dato

#define LCD_CLEAR       0x01  //Orden para borrar la pantalla
#define LCD_NO_CURSOR   0x0C  //Orden para eliminar el cursor de la pantalla
#define LCD_CURSOR      0x0E  //Orden para visualizar el cursor en la pantalla
#define LCD_PARPADEAR   0x0D  //Orden para que parpadee el cursor

//Par�metros del conversor AD
#define termostato_caldera   3   //Entrada AD que se refiere a la temperatura de referencia de la caldera
#define temperatura_caldera  2   //Entrada AD que se refiere a la temperatura del agua en la caldera
#define histeresis_caldera   5   //Diferencia de hist�resis para el agua de la caldera
//Para probar en el PROTEUS, poner una histeresis de 10 �C (los potenci�metros simulados no tienen m�s sensibilidad)
#define t_max_caldera        100  //Temperatura m�xima a la que puede regularse la caldera
#define t_min_caldera        0  //Temperatura m�nima a la que puede regularse la caldera
#define AD_num_valores       255 //N�mero de valores de escala del conversor (sensibilidad de los potenci�metros)

//Par�metros de control de temperatura
//Datos suministrados por el aparato de aire acondicionado (con bomba de calor) de mi casa
#define temp_max          32  //Temperatura m�xima de regulaci�n del termostato
#define temp_min          16  //Temperatura m�nima de regulaci�n del termostato

//Direcciones para la EEPROM
#define eeprom_termostato        0x0000  //Valor del termostato
#define eeprom_num_registros     0x0001  //N�mero de registros de desconexi�n de los que se disponen
#define eeprom_anno_actual       0x0003  //A�o en el que nos encontramos
#define eeprom_anno_0_to_3       0x0004  //A�o en el que nos encontramos medido de 0 a 3 (llamado 1_to_3 por razones "hist�ricas")
#define eeprom_programacion      0x0008  //Posici�n a partir de la cual se almacenan la programaci�n de la alarma
#define eeprom_registros         0x0010  //Posici�n a partir de la cual se almacenan los registros
