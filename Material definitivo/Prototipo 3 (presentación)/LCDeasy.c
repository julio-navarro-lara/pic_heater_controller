////----------------------------------------------------------------------------////
// 	Funciones para la gestion del display LCD de la EasyPIC3
//	con el compilador de CCS.
//	Es una adaptación del archivo LCD.c de CCS para las prácticas de la
//	asignatura, y por tanto no distribuible a usuarios no autorizados.
//------------------------------------------------------------------------------////

//  lcd_init()   Inicializa el LCD: debe ser llamada antes que las otras funciones.
//
//  lcd_putc(c)  Visualiza c en la siguiente posición del display.
//               Caracteres especiales de control:
//                      \f  Borrar display
//                      \n  Saltar a la segunda linea
//                      \b  Retroceder una posición.
//
//  lcd_gotoxy(x,y) Selecciona la nueva posicion de escritura en el display.
//                  (la esquina superior izquierda es 1,1)
//
//  lcd_getc(x,y)   Devuelve el caracter de la posicion x,y del display.
//			(esta función no va en la EASYPIC3, pues R/W=0)
//
//  lcd_send_byte(rs,byte)	escribe byte en el registro de datos(con rs=1) o de
//				instrucciones (con rs=0).
//------------------------------------------------------------------------------//
// Conexion del LCD con el PIC:
//     RB0 --
//     RB1 --
//     RB2  rs
//     RB3  Enable
//     RB4  D4
//     RB5  D5
//     RB6  D6
//     RB7  D7
//
// Los pines D0-D3 del LCD no se usan
// El pin RB0 del PIC no se usa
// El pin RB1 no está conectado en la tarjeta,
// podría ser para r/w

// Un-comment the following define to use port B
#define use_portb_lcd TRUE 		//LINEA DES-COMENTADA----------------


struct lcd_pin_map {                 // This structure is overlayed
           int unused : 2 ;	//MODIFICADA PARA EASYPIC--------------------
	   //BOOLEAN rw;		//MODIFICADA PARA EASYPIC--------------------
	   BOOLEAN rs;           // on to an I/O port to gain
           BOOLEAN ENABLE;            // access to the LCD pins.
           		           	// The bits are allocated from
           			          // low order up.  ENABLE will
           int     data : 4;         	// be pin B3-------------------------
        } lcd;


#if defined(__PCH__)
#if defined use_portb_lcd
   #byte lcd = 0xF81                   // This puts the entire structure
#else
   #byte lcd = 0xF83                   // This puts the entire structure
#endif
#else
#if defined use_portb_lcd
   #byte lcd = 6                  // on to port B (at address 6)
#else
   #byte lcd = 8                 // on to port D (at address 8)
#endif
#endif

#if defined use_portb_lcd
   #define set_tris_lcd(x) set_tris_b(x)
#else
   #define set_tris_lcd(x) set_tris_d(x)
#endif


#define lcd_type 2           // 0=5x7, 1=5x10, 2=2 lines
#define lcd_line_two 0x40    // LCD RAM address for the second line


BYTE const LCD_INIT_STRING[4] = {0x24 | (lcd_type << 2), 0xF, 1, 6};
                             // These bytes need to be sent to the LCD
                             // to start it up.


                             // The following are used for setting
                             // the I/O port direction register.

struct lcd_pin_map const LCD_WRITE = {3,0,0,0}; // For write mode all pins are out
struct lcd_pin_map const LCD_READ = {3,0,0,15}; // For read mode data pins are in



BYTE lcd_read_byte() {
      BYTE low,high;
      set_tris_lcd(LCD_READ);
      //////lcd.rw = 1;
      delay_cycles(1);
      lcd.enable = 1;
      delay_cycles(1);
      high = lcd.data;
      lcd.enable = 0;
      delay_cycles(1);
      lcd.enable = 1;
      delay_us(1);
      low = lcd.data;
      lcd.enable = 0;
      set_tris_lcd(LCD_WRITE);
      return( (high<<4) | low);
}


void lcd_send_nibble( BYTE n ) {
      lcd.data = n;
      delay_cycles(1);
      lcd.enable = 1;
      delay_us(2);
      lcd.enable = 0;
}


void lcd_send_byte( BYTE address, BYTE n ) {

      lcd.rs = 0;
      delay_us(3000);
//      while ( bit_test(lcd_read_byte(),7) ) ;
      lcd.rs = address;
      delay_cycles(1);
      //////////lcd.rw = 0;
      delay_cycles(1);
      lcd.enable = 0;
      lcd_send_nibble(n >> 4);
      lcd_send_nibble(n & 0xf);
}


void lcd_init() {
    BYTE i;
    set_tris_lcd(LCD_WRITE);
    lcd.rs = 0;
    /////lcd.rw = 0;
    lcd.enable = 0;
    delay_ms(50);
    //for(i=1;i<=3;++i) {
    //   lcd_send_nibble(3);
    //  delay_ms(5);
    //}
    lcd_send_nibble(2);
    delay_ms(5);
    for(i=0;i<=3;++i)
       { lcd_send_byte(0,LCD_INIT_STRING[i]);
        delay_ms(5); }
}


void lcd_gotoxy( BYTE x, BYTE y) {
   BYTE address;

   if(y!=1)
     address=lcd_line_two;
   else
     address=0;
   address+=x-1;
   lcd_send_byte(0,0x80|address);
}

void lcd_putc( char c) {
   switch (c) {
     case '\f'   : lcd_send_byte(0,1);
                   delay_ms(2);
                                           break;
     case '\n'   : lcd_gotoxy(1,2);        break;
     case '\b'   : lcd_send_byte(0,0x10);  break;
     default     : lcd_send_byte(1,c);     break;
   }
}

char lcd_getc( BYTE x, BYTE y) {
   char value;

    lcd_gotoxy(x,y);
    while ( bit_test(lcd_read_byte(),7) ); // wait until busy flag is low
    lcd.rs=1;
    value = lcd_read_byte();
    lcd.rs=0;
    return(value);
}
