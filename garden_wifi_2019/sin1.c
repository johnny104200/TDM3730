#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
unsigned char *sine_table = NULL;
float db2liner(float dB){
	return pow(10, dB / 20);
}
float liner2db(float liner)
{
    return (20 * log10(liner));
}
int waveform_generate(int freq, int sps, float dB){
	int i;
	int table_length;
	float A;
	float value;
	table_length = sps / freq;
	sine_table = (unsigned char *)malloc(table_length);
	memset(sine_table, 0, table_length);
	A = db2liner(dB);
	for( i = 0; i < table_length; i++){
		value = A * sin(2 * M_PI * i * freq / sps);
		if (value < 1.0) 
	    {            
	        sine_table[i] = (256 / 2) + value * (256 / 2);  /* add DC offset */
		}
		else {
			sine_table[i] = 256 - 1;
		}
	}
	return table_length;
}

int main(){ 
	int i, table_length;    /* generate 1000Hz sine wave, sample rate: 8000Hz, gain: -6dB */ 
	table_length = waveform_generate(1000, 8000, -6); 
	printf("unsigned char sine_table[%d] = { ", table_length);
	for (i = 0; i < table_length; i++)  
	{        
	    printf((i != table_length - 1) ? "0x%02X, " : "0x%02X ", sine_table[i]); 
	} 
	printf("};\n");  
	free(sine_table);
	return 0;
}

