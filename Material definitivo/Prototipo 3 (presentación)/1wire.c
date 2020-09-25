/***********************1Wire Class***********************/
/*Funciones b�sicas para comunicaci�n con un solo sensor 1wire */
/*********************************************************/
/*-------1-wire definitions-------*/

#define ONE_WIRE_PIN PIN_A5 //modificar de acuerdo con la tarjeta


/************onewire_reset*************************************************/
void onewire_reset()  // OK if just using a single permanently connected device
{
 output_low(ONE_WIRE_PIN);
 delay_us( 500 ); // pull 1-wire low for reset pulse
 output_float(ONE_WIRE_PIN); // float 1-wire high
 delay_us( 500 ); // wait-out remaining initialisation window.
 output_float(ONE_WIRE_PIN);
}

/*********************** onewire_write() ********************************/
/*Esta funci�n escribe un byte en el sensor.*/
/* */
/*Parametros: byte - el byte a escribir en el 1wire */

void onewire_write(int data)
{
 int count;

 for (count=0; count<8; ++count)
 {
  output_low(ONE_WIRE_PIN);
  delay_us( 2 ); // pull 1-wire low to initiate write time-slot.
  output_bit(ONE_WIRE_PIN, shift_right(&data,1,0)); // set output bit on 1-wire
  delay_us( 60 ); // wait until end of write slot.
  output_float(ONE_WIRE_PIN); // set 1-wire high again,
  delay_us( 2 ); // for more than 1us minimum.
 }
}

/*********************** read1wire() *********************************/
/*Esta funci�n lee un dato de 8 bits del sensor.*/
/*Devuelve: dato de 8-bit (1-byte)*/

int onewire_read()
{
 int count, data;

 for (count=0; count<8; ++count)
 {
  output_low(ONE_WIRE_PIN);
  delay_us( 2 ); // pull 1-wire low to initiate read time-slot.
  output_float(ONE_WIRE_PIN); // now let 1-wire float high,
  delay_us( 8 ); // let device state stabilise,
  shift_right(&data,1,input(ONE_WIRE_PIN)); // and load result.
  delay_us( 120 ); // wait until end of read slot.
 }

 return( data );
}

///////////////////////////////////////////////////////////////////


