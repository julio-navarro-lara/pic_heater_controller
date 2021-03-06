

const char caracteres[12]={'1','2','3','4','5','6','7','8','9','S','0','N'};

char find_key();

//Devuelve la tecla pulsada, esperando a que el usuario la suelte
char get_key()
{
   char key;
   key=find_key();
   //Si se pulsa alguna tecla, nos esperamos hasta que se suelte para recoger el valor
   if(key)
      while(find_key() != 0);
   return key;
}

char find_key()
{
   int8 fila, columna, temp, contador, puertoB;
   char key = 0;
   
   //Ponemos puerto D para input y puerto B para output (parcialmente)
   set_tris_b(0x00);
   set_tris_d(0x07);
   
   //Recorremos las filas forzando un valor con el puerto B para detectar
   //la tecla pulsada
   for(fila=0; fila < NUM_FILAS; fila++){
      
      //Forzamos la salida de B a que tenga todo 1 menos la posici�n a inspeccionar
      puertoB = ~(1 << fila);
      teclado1 = puertoB & 0b1;
      teclado2 = (puertoB & 0b10) >> 1;
      teclado3 = (puertoB & 0b100) >> 2;
      teclado4 = (puertoB & 0b1000) >> 3;
      
      //Introducimos un poco de retraso
      for(contador=0; contador<100; contador++);
      
      //Guardamos el valor que aparece en el puerto D
      //(en sus 3 bits menos significativos)
      temp = port_d & 0b00000111;
      
      //Iteramos en base a esto para ver si alguna columna est� activa
      //Una vez localizada la columna, podemos encontrar el caracter teniendo en cuenta
      //tambi�n la fila que est� activa.
      for(columna=0; columna < NUM_COLUMNAS; columna++){
      
         //Realizamos la operaci�n AND entre el valor que hay y un 1 desplazado
         //seg�n la columna que nos encontremos
         if((temp & (1 << columna))==0)
         {
            //Si da 0, hemos localizado la columna d�nde se encuentra la tecla
            //Guardamos su �ndice teniendo en cuenta que las teclas se numeran de izquierda a derecha y
            //de arriba a abajo.
            int8 index;
            index = (fila*NUM_COLUMNAS)+(2-columna);
            
            //Buscamos la tecla a la que se hace referencia
            key = caracteres[index];
            
            //Saltamos para salirnos del bucle
            goto hecho;
         
         }
      
      }
   }
   
   hecho:
   
   //Devolvemos su estado al puerto B
   set_tris_b(0);
   return key;

}
