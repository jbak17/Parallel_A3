/******************************************************************************

	The program reads an BMP image file and creates a new
	image with gaussian blur applied.

******************************************************************************/
#include <time.h>
#include <stdlib.h>
#include <stdio.h>

#include "gaussianLib.h"	/*Our Bitmap operations library */
#include "mpi.h"
#include "bmp.h"

#define KERNEL_DIMENSION_SD 3
#define KERNEL_BITMAP_FILENAME "kernel.bmp"
#define ARGS 4
#define MASTER 0
#define DEBUG 1
#define B_SIZE 10000

/******************************************************************************
* main
*
* Demo program for applying a gaussian blur filter to a bitmap image. This
* program uses qdbmp library for bitmap operations.
*
* Usage: gaussian_blur <input file> <output file> <standard deviation>
*
* Compile: make
*
* This program:
 *  1 - declares a bunch of variables
 *  2 - gets the arguments
 *  3 - creates a kernel
 *  4 - reads in an image
 *  --- starts timer ---
 *  5 - creates a new image
 *  6 - applies gaussian blur of old image, saving results in new image
 *  --- stops timer ---
 *  7 - writes results to file
 *  8 - frees used memory
 *
 *  The master process (process 0) will need to read in the source
	bitmap image (name speci�ed as a command-line argument) and send all required
	data to each process. Process 0 will then need to receive the outputs from the
	worker processes (well actually only one process if you use a tree structure) and
	construct the output image.

 	MPI_Scatter - sends data out to processes
 	MPI_Gather - collects the data from a group of processes.

 	We want to scatter the image and then gather all the items back in again.


*
******************************************************************************/
// Simple Debug Feedback - writes to a simple text file.
void debug(char* str)
{
	FILE* fp = fopen("info.txt", "a+");
	fprintf(fp, "%s\n", str);
	fclose(fp);
}// End of debug(..)

char buf[500]; // Temp buffer for text output

/*
 * Turns a bmp image into a 1D array
 *
 * in:      bmp image file pointer
 *          offset to skip over header
 *          height of bmp
 *          width of picture
 * out:     array of color data
 */
void bmpToArray(FILE *fd, unsigned int* data, int offset, int width, int height){

	//skip over metadata: ready to read color pixels
	fseek( fd, offset, SEEK_SET);

	// Check if padding is needed for bmp format
	int PaddedBytes = (width * 3) % 4;
	sprintf(buf, "PaddedBytes: %d\n\n", PaddedBytes);
	debug(buf);

	for(unsigned int  h=0; h<height; h++) {

		for(unsigned int  w=0; w<width; w++) {
			/* So in this loop - if our data isn't aligned to 4 bytes, then its been padded
			* in the file so it aligns...so we check for this and skip over the padded 0's
			* The data is read in as b,g,r and not rgb.
			 * */
			unsigned char r,g,b;
			fread(&b, 1, 1, fd);
			fread(&g, 1, 1, fd);
			fread(&r, 1, 1, fd);
			data[ w + h*width ] = (r<<16 | g<<8 | b);
		}// End of for loop w

		//If there are any padded bytes - we skip over them here
		if( PaddedBytes != 0 )
		{
			unsigned char skip[4];
			fread(skip, 1, 4 - PaddedBytes, fd);
		}// End of if reading padded bytes
	}// End of for loop h

	if(DEBUG){
		sprintf(buf, "%d pixels read\n\n", (width * height));
		debug(buf);
	}

}

/*
 * Receive commmand line inputs:
 *
 * In : me
 *
 * Out: old_bmp; image to process
 * 		sd; standard deviation
 *
 */
int
parse_args (int argc, char *argv[], int *sd)
{
	if (argc != ARGS)
	{
		fprintf (stderr,
				 "Usage: %s <input file> <output file> <standard deviation>\n",
				 argv[0]);
		return 0;
	}



	if ((*sd = atoi(argv[3])) <= 0){
		fprintf (stderr, "Standard deviation must be a number.\n");
		return 0;
	}

	return (0);
}

int
main (int argc, char *argv[])
{

	/**********************************************
				DECLARATIONS

	**********************************************/

	BMP *bmp; BMP *new_bmp;
	unsigned int *color_data; //storage for pixels
	float colour_max, kernel_max;
	int i;
	int width, height, offset; //for images
	int sd, origin; //for kernel
	float kernel_dim, **kernel; //pointer to array
	int me, nproc; //for parallelism

	BITMAPFILEHEADER bf;
	BITMAPINFOHEADER bi;


	/**********************************************
				SET UP PARALLEL ENVIRONMENT

	**********************************************/

	/* Check arguments - You should check types as well! */
	parse_args(argc, argv, &sd);

	MPI_Init(&argc, &argv);
	MPI_Comm_rank(MPI_COMM_WORLD, &me);
	MPI_Comm_size(MPI_COMM_WORLD, &nproc);

	//information for pack/unpack functionality
	unsigned int buff[B_SIZE];
	int position = 0;

	if(me == MASTER){

		// remember filenames
		char *infile = argv[1];

		/* Read an image file */
		FILE *inptr = fopen(infile, "r");
		if (inptr == NULL)
		{
			printf("Could not open %s.\n", infile);
			return 2;
		}

		//get metadata about image
		fread(&bf, sizeof(BITMAPFILEHEADER), 1, inptr);
		fread(&bi, sizeof(BITMAPINFOHEADER), 1, inptr);

		width = bi.biWidth;
		height = bi.biHeight;
		offset = bf.bfOffBits;
		int metadataNo = 2;
		//B_SIZE = width*height + metadataNo; // +2 for transfer of metadata

		// Allocate some memory for our image bits and transfer buffer
		unsigned int *imgData = (unsigned int*) malloc((width) * height * sizeof(unsigned int));
		//buff = (unsigned int*) malloc(B_SIZE * sizeof(unsigned int));
		//get color data
		bmpToArray(inptr, imgData, offset, width, height);

		//send height, width and color data
		MPI_Pack(&width, 1, MPI_INT, buff, B_SIZE, &position, MPI_COMM_WORLD);
		MPI_Pack(&height, 1, MPI_INT, buff, B_SIZE, &position, MPI_COMM_WORLD);
		MPI_Pack(imgData, width*height, MPI_UNSIGNED, buff, B_SIZE, &position, MPI_COMM_WORLD);
		MPI_Bcast(buff, B_SIZE, MPI_PACKED, MASTER, MPI_COMM_WORLD);
	} else {
	//slave processes get data
		//buff = (unsigned int*) malloc(B_SIZE * sizeof(unsigned int)); //todo fix hard coding
		MPI_Bcast(buff, B_SIZE, MPI_PACKED, MASTER, MPI_COMM_WORLD);

		MPI_Unpack(buff, B_SIZE, &position, &width, 1, MPI_INT, MPI_COMM_WORLD);
		MPI_Unpack(buff, B_SIZE, &position, &height, 1, MPI_INT, MPI_COMM_WORLD);
		// Allocate some memory for this process' pixels

		color_data = (unsigned int*) malloc(width * height * sizeof(unsigned int));
		MPI_Unpack(buff, B_SIZE, &position, color_data, (width*height), MPI_UNSIGNED, MPI_COMM_WORLD);
	}
	if(DEBUG) {
		sprintf(buf, "This is process %d of %d and I can read %d and %d.\n\n", me, nproc, height, width);
		debug(buf);
	}//end debug

	/**********************************************
				KERNEL CODE

	**********************************************/

	/*The kernel dimensions are deterined by th sd. Pixels beyond 3 standard deviations have
		practiaclly no impact on the value for the origin cell */
	kernel_dim = (2 * (KERNEL_DIMENSION_SD * sd)) + 1;
	/*The center cell of the kernel */
	origin = KERNEL_DIMENSION_SD * sd;

	/* Now Lets allocate our Gaussian Kernel - The dimensions of the kernel will be 2*3*sd by 2*3*sd) */
	kernel = (float **) malloc (kernel_dim * sizeof (float *));
	for (i = 0; i < kernel_dim; i++)
	{
		kernel[i] = (float *) malloc (kernel_dim * sizeof (float));
	}

	if(DEBUG || (me == MASTER)) {
		sprintf(buf, "Generating kernel\n");
		debug(buf);
	}
	/* Lets generate or kernel based upon the specs */
	generateGaussianKernel(kernel, kernel_dim, sd, origin, &kernel_max,
						   &colour_max);


		applyConvolution(kernel, kernel_dim, origin, colour_max, bmp, new_bmp, 1);

		if (me == MASTER) {
			/* Save result */
			BMP_WriteFile(new_bmp, argv[2]);
			BMP_CHECK_ERROR (stdout, -2);

			/* Free all memory allocated for the image */
			BMP_Free(bmp);
			BMP_Free(new_bmp);
		}


		/**********************************************
					CLEANUP CODE

		**********************************************/
		/* Free the kernel memory */
		for (i = 0; i < kernel_dim; i++) {
			free(kernel[i]);
		}
		free(kernel);

		MPI_Finalize();
		return 0;
	}







