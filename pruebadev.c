#include <stdio.h>


main(){
       
   int num_registros = 8;
   
   int registros[8][9]={{31, 12, 12, 6, 20, 0, 1, 0, 9},{31, 12, 13,34,18,50,2,1,9},{31,12,13,41,16,50,2,1,9},
   {31,12,13,48,16,50,2,0,9},{31,12,17,40,16,50,2,2,9},{2,1,22,55,16,50,1,0,10},{2,1,23,06,21,50,10,0,10},{4,1,23,56,24,0,1,1,10}};
       
       
   int contador, contador2;
   int data[8];
   int anno;
   float media_dia_on; //Tiempo medio en minutos que se enciende por día
   float media_dia_mes[12]; //Tiempos medios en minutos que se enciende en cada mes
   int dias_meses[12]; //Número de dias que tiene cada mes
   float media_dia_caldera; //Tiempo medio que se enciende la caldera
   float media_dia_mes_caldera[12]; //Tiempo medio que se enciende la caldera en cada mes

   //Capacidad para 4 años de datos
   float media_annos[4][12];
   float media_annos_caldera[4][12];
   int num_annos;
   int annos[4];

   //Para almacenar valores provisionales
   long tiempo_x_dia;
   long tiempo_x_dia_caldera;
   int dia[3];
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
   printf("Momento de apagado     Temperatura        Minutos encendido       Minutos caldera\n");
   printf("=================================================================================\n");
   //Poner encabezamiento

   for(contador=0; contador<num_registros; contador++)
   {
      for(contador2=0; contador2<8; contador2++)
      {
         data[contador2]=registros[contador][contador2];                 
                       
      }

      anno=registros[contador][8];
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

      printf("%u\n", data[7]);

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
   
   printf("%f",media_dia_on);
   
   while(1){}   
       
       




}
