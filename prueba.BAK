#include "18F4520.H"

#use delay(clock=8000000, restart_wdt)

#use fast_io(A)
#use fast_io(B)
#use fast_io(C)
#use fast_io(D)

#byte port_a = 0xF80

#bit motor = 0xF80.4


void main()
{
   setup_adc_ports(NO_ANALOGS);
   set_tris_a(0x0);

   output_bit(PIN_A4,1);
   output_a(0b11111111);

   delay_us(10000);
   
   while(1){};


}
