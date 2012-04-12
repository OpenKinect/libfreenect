/*
 * This file is part of the OpenKinect Project. http://www.openkinect.org
 *
 * Copyright (c) 2010 individual OpenKinect contributors. See the CONTRIB file
 * for details.
 *
 * This code is licensed to you under the terms of the Apache License, version
 * 2.0, or, at your option, the terms of the GNU General Public License,
 * version 2.0. See the APACHE20 and GPL2 files for the text of the licenses,
 * or the following URLs:
 * http://www.apache.org/licenses/LICENSE-2.0
 * http://www.gnu.org/licenses/gpl-2.0.txt
 *
 * If you redistribute this file in source form, modified or unmodified, you
 * may:
 *   1) Leave this header intact and distribute it under the same terms,
 *      accompanying it with the APACHE20 and GPL20 files, or
 *   2) Delete the Apache 2.0 clause and accompany it with the GPL2 file, or
 *   3) Delete the GPL v2 clause and accompany it with the APACHE20 file
 * In all cases you must keep the copyright notice intact and include a copy
 * of the CONTRIB file.
 *
 * Binary distributions must follow the binary distribution requirements of
 * either License.
 *
 */

#include <sys/stat.h>
#include <sys/types.h>
#include <signal.h>
#include <sys/time.h>
#include <math.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <libfreenect/libfreenect-registration.h>

#define DEPTH_MAX_METRIC_VALUE 10000
#define DEPTH_MAX_RAW_VALUE 2048
#define DEPTH_NO_MM_VALUE 0

#define DEPTH_X_RES 640
#define DEPTH_Y_RES 480

#define REG_X_VAL_SCALE 256 // "fixed-point" precision for double -> int32_t conversion
#define DEPTH_MIRROR_X 0

void dump_registration(char* regfile) {
  printf("Dumping Kinect registration to %s\n", regfile);

  freenect_context *f_ctx;
  if (freenect_init(&f_ctx, NULL) < 0) {
	printf("freenect_init() failed\n");
	exit(0);
	//return 1;
  }

  freenect_set_log_level(f_ctx, FREENECT_LOG_DEBUG);
  freenect_select_subdevices(f_ctx, 
          (freenect_device_flags)(FREENECT_DEVICE_MOTOR | FREENECT_DEVICE_CAMERA));

  freenect_device *f_dev;
  int user_device_number = 0;
  if (freenect_open_device(f_ctx, &f_dev, user_device_number) < 0) {
	printf("Could not open device\n");
	exit(0);
	//return 1;
  }

  freenect_registration dev;
  dev = freenect_copy_registration( f_dev );

  FILE* fp = fopen(regfile, "w");
  if (!fp) {
	printf("Error: Cannot open file '%s'\n", regfile);
	exit(1);
  }

  // from freenect_copy_registration in registration.c
  /*
  dev.reg_info = dev->registration.reg_info;
  dev.reg_pad_info = dev->registration.reg_pad_info;
  dev.zero_plane_info = dev->registration.zero_plane_info;
  dev.const_shift = dev->registration.const_shift;
  dev.raw_to_mm_shift    = (uint16_t*)malloc( sizeof(uint16_t) * DEPTH_MAX_RAW_VALUE );
  dev.depth_to_rgb_shift = (int32_t*)malloc( sizeof( int32_t) * DEPTH_MAX_METRIC_VALUE );
  dev.registration_table = (int32_t (*)[2])malloc( sizeof( int32_t) * DEPTH_X_RES * DEPTH_Y_RES * 2 );
  */

  /* Should be ~2.4 MB
  printf( "\n\n%d\n\n", sizeof(dev)+
		  sizeof(uint16_t)*DEPTH_MAX_RAW_VALUE + 
		  sizeof(int32_t)*DEPTH_MAX_METRIC_VALUE + 
		  sizeof(int32_t)*DEPTH_X_RES*DEPTH_Y_RES*2 );
  */

  // write out first four fields
  fwrite( &dev.reg_info, sizeof(dev.reg_info), 1, fp );
  fwrite( &dev.reg_pad_info, sizeof(dev.reg_pad_info), 1, fp);
  fwrite( &dev.zero_plane_info, sizeof(dev.zero_plane_info), 1, fp);
  fwrite( &dev.const_shift, sizeof(dev.const_shift), 1, fp);

  //printf( "\n\n%d\n\n", sizeof(uint16_t)*DEPTH_MAX_RAW_VALUE ); // 4096
  fwrite( (&dev)->raw_to_mm_shift, sizeof(uint16_t), DEPTH_MAX_RAW_VALUE, fp );

  //printf( "\n\n%d\n\n", sizeof(int32_t)*DEPTH_MAX_METRIC_VALUE ); // 4000
  fwrite( (&dev)->depth_to_rgb_shift, sizeof(int32_t), DEPTH_MAX_METRIC_VALUE, fp );

  //printf( "\n\n%d\n\n", sizeof(int32_t)*DEPTH_X_RES*DEPTH_Y_RES*2 ); // 2457600
  fwrite( (&dev)->registration_table, sizeof(int32_t), DEPTH_X_RES*DEPTH_Y_RES*2, fp );

  fclose(fp);
}


// From: https://github.com/floe/libfreenect/blob/ee8666614e9ff620f49c83f466e5a99f6a0be4a0/src/registration.c
int freenect_apply_registration(freenect_registration* reg, uint16_t* input_raw, uint16_t* output_mm) {

  memset(output_mm, DEPTH_NO_MM_VALUE, DEPTH_X_RES * DEPTH_Y_RES * sizeof(uint16_t)); // clear the output image
  uint32_t target_offset = DEPTH_Y_RES * reg->reg_pad_info.start_lines;
  uint32_t x,y,source_index = 0;
  
  for (y = 0; y < DEPTH_Y_RES; y++) {
	for (x = 0; x < DEPTH_X_RES; x++) {
	  
	  // get the value at the current depth pixel, convert to millimeters
	  uint16_t metric_depth = reg->raw_to_mm_shift[ input_raw[source_index++] ];
	  
	  // so long as the current pixel has a depth value
	  if (metric_depth == DEPTH_NO_MM_VALUE) continue;
	  if (metric_depth >= DEPTH_MAX_METRIC_VALUE) continue;
	  
	  // calculate the new x and y location for that pixel
	  // using registration_table for the basic rectification
	  // and depth_to_rgb_shift for determining the x shift
	  uint32_t reg_index = DEPTH_MIRROR_X ? ((y + 1) * DEPTH_X_RES - x - 1) : (y * DEPTH_X_RES + x);
	  uint32_t nx = (reg->registration_table[reg_index][0] + reg->depth_to_rgb_shift[metric_depth]) / REG_X_VAL_SCALE;
	  uint32_t ny = reg->registration_table[reg_index][1];
	  
	  // ignore anything outside the image bounds
	  if (nx >= DEPTH_X_RES) continue;
	  
	  // convert nx, ny to an index in the depth image array
	  uint32_t target_index = (DEPTH_MIRROR_X ? ((ny + 1) * DEPTH_X_RES - nx - 1) : (ny * DEPTH_X_RES + nx)) - target_offset;
	  
	  // get the current value at the new location
	  uint16_t current_depth = output_mm[target_index];
	  
	  // make sure the new location is empty, or the new value is closer
	  if ((current_depth == DEPTH_NO_MM_VALUE) || (current_depth > metric_depth)) {
		output_mm[target_index] = metric_depth; // always save depth at current location
	  }
	}
  }
  return 0;
}

freenect_registration load_registration(char* regfile) 
{

  FILE *fp = NULL;
  /* load the regdump file */
  fp = fopen(regfile, "r");
  if (!fp) {
	perror(regfile);
	exit(1);
  }
  
  freenect_registration reg;
  // allocate memory and set up pointers
  // (from freenect_copy_registration in registration.c)
  reg.raw_to_mm_shift    = (uint16_t*)malloc( sizeof(uint16_t) * DEPTH_MAX_RAW_VALUE );
  reg.depth_to_rgb_shift = (int32_t*)malloc( sizeof( int32_t) * DEPTH_MAX_METRIC_VALUE );
  reg.registration_table = (int32_t (*)[2])malloc( sizeof( int32_t) * DEPTH_X_RES * DEPTH_Y_RES * 2 );
  // load (inverse of kinect_regdump)
  fread( &reg.reg_info, sizeof(reg.reg_info), 1, fp );
  fread( &reg.reg_pad_info, sizeof(reg.reg_pad_info), 1, fp);
  fread( &reg.zero_plane_info, sizeof(reg.zero_plane_info), 1, fp);
  fread( &reg.const_shift, sizeof(reg.const_shift), 1, fp);
  fread( reg.raw_to_mm_shift, sizeof(uint16_t), DEPTH_MAX_RAW_VALUE, fp );
  fread( reg.depth_to_rgb_shift, sizeof(int32_t), DEPTH_MAX_METRIC_VALUE, fp );
  fread( reg.registration_table, sizeof(int32_t), DEPTH_X_RES*DEPTH_Y_RES*2, fp );
  fclose(fp);

  return reg;
}


void load_PGM(char *PGMfile, uint16_t* data)
{
  FILE *fp = NULL;
  fp = fopen(PGMfile, "r");
  if (!fp) {
	perror(PGMfile);
	exit(1);
  }
  while (getc(fp) != '\n'); // skip header line
  fread(data, sizeof(uint16_t), DEPTH_X_RES*DEPTH_Y_RES, fp);
  fclose(fp);
}

/*
void write_PGM(char *PGMfile, uint16_t* data, char *type)
{
  // make filename <whatever>.dist.pgm
  char *PGMfile_out = (char *)malloc(strlen(PGMfile) + 2); // '.x'
  sprintf(PGMfile_out, "%s", PGMfile);
  sprintf(PGMfile_out+strlen(PGMfile)-3, "%s", type);
  sprintf(PGMfile_out+strlen(PGMfile)-2, "%s", ".pgm");
  
  FILE *fp = NULL;
  fp = fopen(PGMfile_out, "w");
  if (!fp) {
	perror(PGMfile_out);
	exit(1);
  }

  fprintf(fp, "P5 %d %d 65535\n", DEPTH_X_RES, DEPTH_Y_RES );
  fwrite(data, sizeof(uint16_t), DEPTH_X_RES*DEPTH_Y_RES, fp);
  fclose(fp);
}
*/


void write_xyz_bin(char *infile, double* x, double* y, uint16_t* z)
{
  FILE* fp;
  // make filename <whatever>.x
  char *file_out = (char *)malloc(strlen(infile) + 10);
  sprintf(file_out, "%s", infile);

  // X
  sprintf(file_out+strlen(infile)-3, "%s", "x");
  fp = NULL;
  fp = fopen(file_out, "w");
  if (!fp) {
	perror(file_out);
	exit(1);
  }
  fwrite(x, sizeof(double), DEPTH_X_RES*DEPTH_Y_RES, fp);
  fclose(fp);

  // Y
  sprintf(file_out+strlen(infile)-3, "%s", "y");
  fp = NULL;
  fp = fopen(file_out, "w");
  if (!fp) {
	perror(file_out);
	exit(1);
  }
  fwrite(y, sizeof(double), DEPTH_X_RES*DEPTH_Y_RES, fp);
  fclose(fp);

  // Z
  sprintf(file_out+strlen(infile)-3, "%s", "z");
  fp = NULL;
  fp = fopen(file_out, "w");
  if (!fp) {
	perror(file_out);
	exit(1);
  }
  fwrite(z, sizeof(uint16_t), DEPTH_X_RES*DEPTH_Y_RES, fp);
  fclose(fp);
}



void apply_registration(char* regfile, char* PGMfile)
{
  int i, j;

  freenect_registration reg;
  reg = load_registration(regfile);

  uint16_t data[DEPTH_X_RES*DEPTH_Y_RES];
  load_PGM(PGMfile, data);

  /* Convert DN to world */
  // first, convert the DN to worldz
  double wx[DEPTH_X_RES*DEPTH_Y_RES], wy[DEPTH_X_RES*DEPTH_Y_RES];
  uint16_t wz[DEPTH_X_RES*DEPTH_Y_RES];
  freenect_apply_registration(&reg, data, wz);

  // then, convert x and y to world_x and world_y
  //printf("Converting camera to world coordinates\n");

  // see freenect_camera_to_world() in registration.c
  double ref_pix_size = reg.zero_plane_info.reference_pixel_size;
  double ref_distance = reg.zero_plane_info.reference_distance;
  double factor;
  uint32_t idx = 0;
  for (j=0; j<DEPTH_Y_RES; j++) {
	for (i=0; i<DEPTH_X_RES; i++) {
	  // freenect_camera_to_world(&reg, i, j, data[i*j], &wwx, &wwy);
	  // internals of freenect_camera_to_world in registration.c
	  factor = 2 * ref_pix_size * wz[idx] / ref_distance;
	  wx[idx] = (double)(i - DEPTH_X_RES/2) * factor;
	  wy[idx] = (double)(j - DEPTH_Y_RES/2) * factor;
	  idx++;
	} // j
  } // i


  /* Write out x,y,z binary files */
  write_xyz_bin(PGMfile, wx, wy, wz);

  /* Write out x,y,z ASCII file */
  //printf("Writing xyz file\n");
  char *outfile_ascii = (char *)malloc(strlen(PGMfile) + 3);
  sprintf(outfile_ascii, "%s", PGMfile ); // append '.xyz' to input filename
  sprintf(outfile_ascii+strlen(PGMfile)-3, "%s", "xyz");
  FILE *fp;
  fp = fopen(outfile_ascii, "w");
  if (!fp) {
	perror(outfile_ascii);
	exit(1);
  }

  for (i=0; i<(DEPTH_X_RES*DEPTH_Y_RES); i++) {

	if (wx[i] == 0 && wy[i] == 0)
	  continue;

	if (wz[i] == 0)
	  continue;

	fprintf(fp, "%d %d %d\n", (int16_t)wx[i], (int16_t)wy[i], wz[i]);
  }
  fclose(fp);
  //printf("Wrote xyz file\n");

}



void usage()
{
  printf("Kinect Offline Registration\n");
  printf("Usage:\n");
  printf("  kinect_register [-h] [-s <regfile> | -a <regfile> <PGM>\n");
  printf(" -h: Display this help message\n"
		 " -s: Save the registration parameters from a connected Kinect to <regfile>\n"
		 " -a: Apply the registration parameters from <regfile> to <PGM> (from 'record')\n"
		 );
  exit(0);
}

int main(int argc, char **argv)
{
  int c=1;
  char *regfile=0, *PGMfile=0;
  if (argc == 1)
	usage();

  while (c < argc) {
	if (strcmp(argv[c],"-h")==0)
	  usage();
	else if (strcmp(argv[c],"-s")==0) {
	  if (argc <= (c+1))
		usage();
	  regfile = argv[c+1];
	}
	else if (strcmp(argv[c],"-a")==0) {
	  if (argc <= (c+2))
		usage();
	  regfile = argv[c+1];
	  PGMfile = argv[c+2];
	}
	c++;
  }

  if (regfile != 0 && PGMfile == 0)
	dump_registration(regfile);
  else
	apply_registration(regfile, PGMfile);
  
  return 0;
}
