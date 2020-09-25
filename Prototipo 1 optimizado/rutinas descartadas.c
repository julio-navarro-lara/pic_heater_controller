
//Rutina de inicializaci�n del prototipo de Proteus a partir del lcd
void inicializacion()
{
   //Variables para introducir los datos de fecha y hora
   int weekday, dia, mes, anno, horas, minutos;
   //Variable para escritura en memoria
   int data[8];
   char c[1]; //Cadena para la conversi�n a int

   //Comenzamos pidiendo la introducci�n de la fecha
   e1:
   lcd_send_byte(LCD_ORDEN, LCD_CLEAR);
   lcd_send_byte(LCD_ORDEN, LCD_CURSOR);
   printf(lcd_putc, " FECHA:\n LUN   /  /  ");

   //Leemos el d�a de la semana
   weekday = 0;
   lcd_gotoxy(2,2);
   //Caracter inutilizado para que no haya problemas
   key='J';

   while(1){
      //Buscamos la introducci�n de un n�mero de d�a
      key = get_key();
      //Si la tecla corresponde a uno de los d�as de la semana (de 1 a 7), cambiamos el d�a.
      if(key=='1' || key=='2' || key=='3' || key=='4' || key=='5' || key=='6' || key=='7'){
         //Restamos 1 por medirse los d�as de la semana de 0 a 6 en el reloj
         c[0]=key;
         weekday = atoi(c)-1;
         key = 'J';

         //Elegimos el nombre del d�a de la semana a mostrar seg�n el n�mero
         switch(weekday)
         {
            case 0: printf(lcd_putc, "LUN");
                    break;
            case 1: printf(lcd_putc, "MAR");
                    break;
            case 2: printf(lcd_putc, "MIE");
                    break;
            case 3: printf(lcd_putc, "JUE");
                    break;
            case 4: printf(lcd_putc, "VIE");
                    break;
            case 5: printf(lcd_putc, "SAB");
                    break;
            case 6: printf(lcd_putc, "DOM");
         }

         //Retornamos el cursor a su posici�n
         lcd_gotoxy(2,2);
      }

      //Si se pulsa SI, confirmamos que ese es el d�a de la semana que queremos introducir
      if(key=='S')
      {
         break;
      }
   }

   //Leemos el d�a del mes
   lcd_gotoxy(6,2);
   dia = buscar_numero();
   if(dia>31 || dia==0)
   {
      //Si pulsamos NO o SI, reseteamos la operaci�n
      //(los c�digos de NO y Si son mayores que 31)
      //Tambi�n se resetea si el dia no es v�lido
      delay_ms(LCD_T_RETARDO);
      goto e1;
   }
   //Leemos el mes
   lcd_gotoxy(9,2);
   mes = buscar_numero();
   if(mes>12 || mes==0)
   {
      delay_ms(LCD_T_RETARDO);
      goto e1;
   }

   //Si el d�a introducido no se encuentra en el mes seleccionado, mostramos un mensaje
   //de error y reseteamos la operaci�n
   if((dia>29 && mes==2)||(dia==31 && (mes==4 || mes==6 || mes==9 || mes==11)))
   {
      lcd_send_byte(LCD_ORDEN, LCD_CLEAR);
      lcd_send_byte(LCD_ORDEN, LCD_NO_CURSOR);
      printf(lcd_putc, " Error en\n la fecha");
      delay_ms(LCD_T_RETARDO);
      goto e1;
   }

   //Leemos el a�o
   lcd_gotoxy(12,2);
   anno = buscar_numero();
   delay_ms(LCD_T_RETARDO);
   if(anno>99 || anno<9)
      //Si pulsamos NO o SI, reseteamos la operaci�n
      //Recordemos que los c�digos de NO y SI son mayores que 99
      //Tampoco se permite un a�o menor que 2009 (a�o de fabricaci�n
      //del sistema de control)
      goto e1;

   //Si el a�o no es bisiesto y hemos seleccionado el 29 de febrero,
   //mostramos un mensaje y reseteamos la operaci�n.
   if(anno%4!=0 && mes==2 && dia==29)
   {
      lcd_send_byte(LCD_ORDEN, LCD_CLEAR);
      lcd_send_byte(LCD_ORDEN, LCD_NO_CURSOR);
      printf(lcd_putc, " El anno no\n es bisiesto");
      delay_ms(LCD_T_RETARDO);
      goto e1;
   }

   //Calculamos el a�o en el intervalo de 0 a 3 seg�n sean bisiesto este a�o
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
   //Se pide la introducci�n de la hora
   lcd_send_byte(LCD_ORDEN, LCD_CLEAR);
   lcd_send_byte(LCD_ORDEN, LCD_CURSOR);
   printf(lcd_putc, "     HORA\n       :  ");

   //Leemos la hora
   lcd_gotoxy(6,2);
   horas = buscar_numero();
   if(horas==NO)
      //Si se pulsa el bot�n NO, se resetea todo el proceso
      goto e1;
   if(horas>23)
   {
      //Si se pulsa el bot�n SI o si la hora no est� dentro del
      //rango permitido, se resetea el proceso de introducci�n de la misma
      delay_ms(LCD_T_RETARDO);
      goto e2;
   }
   //Leemos los minutos
   lcd_gotoxy(9,2);
   minutos = buscar_numero();
   delay_ms(LCD_T_RETARDO);
   if(minutos==NO)
      goto e1;
   if(minutos>59)
      goto e2;

   //Almacenamos todo en la estructura que contiene la informaci�n del tiempo
   tiempo.month   = mes;
   tiempo.day     = dia;
   tiempo.year    = anno_actual_0_to_3;
   tiempo.hours   = horas;
   tiempo.minutes = minutos;
   tiempo.seconds = 0x00;
   tiempo.weekday = weekday;

   //Fijamos el reloj a esa fecha y hora
   PCF8583_set_datetime(&tiempo);

   //Establecemos los par�metros iniciales del sistema para resetear la memoria
   termostato1 = termostato2 = (float)20;
   num_intervalos = 0;
   num_registros = 0;
   anno_actual = anno;

   //Escribimos los datos en la EEPROM, almacenando antes en un vector para
   //agilizar la escritura por r�faga
   data[0] = (int)termostato1;
   data[1] = num_intervalos;
   data[2] = (int)(num_registros/256);
   data[3] = (int)(num_registros - data[0]*256);
   data[4] = anno_actual;
   data[5] = anno_actual_0_to_3;

   graba_ee(eeprom_termostato, 6, data);

}

//Rutina de inicializaci�n para el prototipo de la placa a partir del lcd
void inicializacion()
{
   //Variables para introducir los datos de fecha y hora
   int weekday, dia, mes, anno, horas, minutos;

   //Comenzamos pidiendo la introducci�n de la fecha
   e1:
   lcd_send_byte(LCD_ORDEN, LCD_CLEAR);
   lcd_send_byte(LCD_ORDEN, LCD_CURSOR);
   printf(lcd_putc, " FECHA:\n LUN 01/01/2009");

   //Leemos el d�a de la semana
   weekday = 0;
   lcd_gotoxy(2,2);

   while(1){
      //Si pulsamos los botones + y -, cambiamos de d�a
      if(!mas || !menos)
      {
         if(!mas)
         {
            //Si es +, aumentamos en 1 el n�mero de d�a
            weekday++;
            //Si llegamos al final de la semana, volvemos al principio
            if(weekday == 7)
               weekday = 0;
            //Esperamos a que el bot�n se suelte
            while(!mas){};
         }
         if(!menos)
         {
            //Si es -, disminuimos en 1 el n�mero de d�a
            weekday--;
            //Si nos pasamos del principio de la semana, nos vamos al final
            if(weekday == 255)
               weekday = 6;
            //Esperamos a que el bot�n se suelte
            while(!menos){};
         }

         //Elegimos el nombre del d�a de la semana a mostrar seg�n el n�mero
         switch(weekday)
         {
            case 0: printf(lcd_putc, "LUN");
                    break;
            case 1: printf(lcd_putc, "MAR");
                    break;
            case 2: printf(lcd_putc, "MIE");
                    break;
            case 3: printf(lcd_putc, "JUE");
                    break;
            case 4: printf(lcd_putc, "VIE");
                    break;
            case 5: printf(lcd_putc, "SAB");
                    break;
            case 6: printf(lcd_putc, "DOM");
         }

         //Retornamos el cursor a su posici�n
         lcd_gotoxy(2,2);
      }

      //Si se pulsa SI, confirmamos que ese es el d�a de la semana que queremos introducir
      if(!si)
      {
         while(!si);
         break;
      }
   }

   //Leemos el d�a del mes
   dia = buscar_numero(6, 1, 31, 1);
   if(dia==NOCODE)
      //Si pulsamos NO, reseteamos la operaci�n
      goto e1;
   //Leemos el mes
   mes = buscar_numero(9, 1, 12, 1);
   if(mes==NOCODE)
      goto e1;

   //Si el d�a introducido no se encuentra en el mes seleccionado, mostramos un mensaje
   //de error y reseteamos la operaci�n
   if((dia>29 && mes==2)||(dia==31 && (mes==4 || mes==6 || mes==9 || mes==11)))
   {
      lcd_send_byte(LCD_ORDEN, LCD_CLEAR);
      lcd_send_byte(LCD_ORDEN, LCD_NO_CURSOR);
      printf(lcd_putc, " Error en\n la fecha");
      delay_ms(LCD_T_RETARDO);
      goto e1;
   }

   //Leemos el a�o
   anno = buscar_numero(14, 9, 99, 9);
   if(anno==NOCODE)
      //Si pulsamos NO, reseteamos la operaci�n
      goto e1;

   //Si el a�o no es bisiesto y hemos seleccionado el 29 de febrero,
   //mostramos un mensaje y reseteamos la operaci�n.
   if(anno%4!=0 && mes==2 && dia==29)
   {
      lcd_send_byte(LCD_ORDEN, LCD_CLEAR);
      lcd_send_byte(LCD_ORDEN, LCD_NO_CURSOR);
      printf(lcd_putc, " El anno no\n es bisiesto");
      delay_ms(LCD_T_RETARDO);
      goto e1;
   }

   //Calculamos el a�o en el intervalo de 0 a 3 seg�n sean bisiesto este a�o
   //o los anteriores.
   if(anno%4==0)
      anno_actual_0_to_3 = 0;
   else if((anno-1)%4==0)
      anno_actual_0_to_3 = 1;
   else if((anno-2)%4==0)
      anno_actual_0_to_3 = 2;
   else if((anno-3)%4==0)
      anno_actual_0_to_3 = 3;

   //Se pide la introducci�n de la hora
   lcd_send_byte(LCD_ORDEN, LCD_CLEAR);
   lcd_send_byte(LCD_ORDEN, LCD_CURSOR);
   printf(lcd_putc, "     HORA\n     00:00");

   //Leemos la hora
   horas = buscar_numero(6, 0, 23, 0);
   if(horas==NOCODE)
      //Si se pulsa el bot�n NO, se resetea todo el proceso
      goto e1;
   //Leemos los minutos
   minutos = buscar_numero(9, 0, 59, 0);
   if(minutos==NOCODE)
      goto e1;

   //Almacenamos todo en la estructura que contiene la informaci�n del tiempo
   tiempo.month   = mes;
   tiempo.day     = dia;
   tiempo.year    = anno_actual_0_to_3;
   tiempo.hours   = horas;
   tiempo.minutes = minutos;
   tiempo.seconds = 0x00;
   tiempo.weekday = weekday;

   //Fijamos el reloj a esa fecha y hora
   PCF8583_set_datetime(&tiempo);

   //Establecemos los par�metros iniciales del sistema para resetear la memoria
   termostato = 20;
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
}
