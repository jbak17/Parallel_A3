/******************************************************************************

	The program reads an BMP image file and creates a new
	image with gaussian blur applied.

******************************************************************************/

#include "gaussianLib.h"	/*Our Bitmap operations library */
#include "mpi.h"

#define KERNEL_DIMENSION_SD 3
#define KERNEL_BITMAP_FILENAME "kernel.bmp"
#define ARGS 4
#define MASTER 0
#define DEBUG 0

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
int parse_args (int argc, char *argv[], int *sd);

int
main (int argc, char *argv[])
{
	BMP *bmp; BMP *new_bmp;
	float colour_max, kernel_max;
	int i;
	int width, height; //for images
	int sd, origin; //for kernel
	float kernel_dim, **kernel; //pointer to array
	int me, nproc; //for parallelism

	/* Check arguments - You should check types as well! */
	parse_args(argc, argv, &sd);

	MPI_Init(&argc, &argv);
	MPI_Comm_rank(MPI_COMM_WORLD, &me);
	MPI_Comm_size(MPI_COMM_WORLD, &nproc);

	if(me == MASTER){
		/* Read an image file */
		bmp = BMP_ReadFile (argv[1]);
		BMP_CHECK_ERROR (stdout, -1);

		width = BMP_GetWidth (bmp);
		height = BMP_GetHeight (bmp);
		new_bmp = BMP_Create (width, height, 24); //create empty bitmap
	}

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

	/* Lets generate or kernel based upon the specs */
	generateGaussianKernel (kernel, kernel_dim, sd, origin, &kernel_max,
							&colour_max);


	if(DEBUG || (me == MASTER))fprintf(stderr, "Scattering image\n");
	/* Scatter A */
	if(MPI_Scatter(bmp,
				   nproc,
				   MPI_FLOAT,
				   &new_bmp,
				   nproc,
				   MPI_FLOAT,
				   MASTER,
				   MPI_COMM_WORLD) != MPI_SUCCESS){
		fprintf(stderr,"Scattering of A failed\n");
		MPI_Finalize();
		exit(EXIT_FAILURE);
	}

	/* Lets check the runtime performance of our program */

	applyConvolution (kernel, kernel_dim, origin, colour_max, bmp, new_bmp);

	if(me == MASTER){
		/* Save result */
		BMP_WriteFile (new_bmp, argv[2]);
		BMP_CHECK_ERROR (stdout, -2);

		/* Free all memory allocated for the image */
		BMP_Free (bmp);
		BMP_Free (new_bmp);
	}

	/* Free the kernel memory */
	for (i = 0; i < kernel_dim; i++)
	{
		free (kernel[i]);
	}
	free (kernel);

	MPI_Finalize();
	return 0;
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





