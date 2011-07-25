#include "libfreenect.h"
#include <stdio.h>

uint16_t  input[640*480];
uint16_t output[640*480];
	
freenect_registration registration;

// main routine

int main() {

	freenect_init_registration(0,&registration);

	FILE* df = fopen("depth.raw","r");
	if (!df) { printf("could not open depth.raw (expecting 640x480 16-bit depth data)\n"); return 1; }
	fread(input,640*480*2,1,df);
	fclose(df);

	freenect_apply_registration( &registration, input, output );
	
	FILE* cf = fopen("calib.pgm","w+");
	fprintf(cf,"P5 640 480 65535 ");
	fwrite(output,640*480*2,1,cf);
	fclose(cf);

	return 0;
}

