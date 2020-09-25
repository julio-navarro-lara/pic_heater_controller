#byte port_a = 0xF80
#byte port_b = 0xF81
#byte port_c = 0xF82
#byte port_d = 0xF83
#byte port_e = 0xF84

#bit motor = 0xF83.6
#bit caldera_encendida = 0xF83.7
#bit teclado1 = 0xF81.4
#bit teclado2 = 0xF81.5
#bit teclado3 = 0xF81.6
#bit teclado4 = 0xF81.7

//Parámetros de la pantalla LCD
#define LCD_ORDEN   0
#define LCD_DATO    1

#define LCD_APAGAR  0x08

//Parámetros del teclado
#define NUM_COLUMNAS 3
#define NUM_FILAS    4
