#include <stdio.h>
#include <stdlib.h>
unsigned char sine_table[8] = { 0x80, 0xDA, 0xFF, 0xDA, 0x80, 0x25, 0x00, 0x25 };
/* 8KSPS, 0dB */// unsigned char sine_table[8] = { 0x80, 0xAD, 0xC0, 0xAD, 0x80, 0x52, 0x3F, 0x52 }; /* 8KSPS, -6dB */
unsigned char *waveform_data = &sine_table[0];/* for generate 1KHz sine wave, call it per 125us */ 
unsigned char waveform_output(void){
	unsigned char dac_value;
	dac_value = *waveform_data++;
	if (waveform_data >= &sine_table[8])	{
		waveform_data = &sine_table[0];	}	
		return dac_value;
}
int main(){
	int i;
	for (i = 0; i < 16; i++)    {
		printf("%02X\n", waveform_output());    
	}
	return 0;
}

