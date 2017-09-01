
#include "gaussianLib.h"	/*Our Bitmap operations library */
#include "mpi.h"

#define KERNEL_DIMENSION_SD 3
#define KERNEL_BITMAP_FILENAME "kernel.bmp"
#define ARGS 4
#define MASTER 0

int
parse_args (int argc, char *argv[], BMP *old_bmp, int *sd)
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

int main(int argc,  char *argv[]) {

//declare variables
	BMP *bmp; BMP *new_bmp; BMP *temp_bmp;
	float colour_max, kernel_max;
	int i;
	int width, height; //for images
	int sd, origin; //for kernel
	float kernel_dim, **kernel; //pointer to array
	int me, nproc; //for parallelism

	parse_args(argc, argv, bmp, &sd);

//set up parallel environment
	MPI_Init(&argc, &argv);
	MPI_Comm_rank(MPI_COMM_WORLD, &me);
	MPI_Comm_size(MPI_COMM_WORLD, &nproc);

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

	//master reads in image
	if(me == MASTER){
		/* Read an image file */
		bmp = BMP_ReadFile (argv[1]);
		BMP_CHECK_ERROR (stdout, -1);

		//master creates memory space for new image
		width = BMP_GetWidth (bmp);
		height = BMP_GetHeight (bmp);
		new_bmp = BMP_Create (width, height, 24); //create empty bitmap
	}
	MPI_Bcast(&width, 1, MPI_INT, MASTER, MPI_COMM_WORLD);
	MPI_Bcast(&height, 1, MPI_INT, MASTER, MPI_COMM_WORLD);

	//create buffer to receive image component
	temp_bmp = BMP_Create(width, height, 24);

//scatter image
	MPI_Scatter (
			&bmp,
			width/nproc,
			MPI_FLOAT,
			&temp_bmp,
			width/nproc,
			MPI_FLOAT,
			MASTER,
			MPI_COMM_WORLD);

//processes apply blur
	applyConvolution (kernel, kernel_dim, origin, colour_max, bmp, temp_bmp);

//gather images
	MPI_Gather(&temp_bmp,
			   width/nproc,
			   MPI_FLOAT,
			   &new_bmp,
			   width/nproc,
			   MPI_FLOAT,
			   MASTER,
			   MPI_COMM_WORLD);


//write to file
	if(me == MASTER){
		/* Save result */
		BMP_WriteFile (new_bmp, argv[2]);
		BMP_CHECK_ERROR (stdout, -2);

		/* Free all memory allocated for the image */
		BMP_Free (bmp);
		BMP_Free (new_bmp);
		BMP_Free (temp_bmp);
	} else {
		BMP_Free (temp_bmp);
	}

	/* Free the kernel memory */
	for (i = 0; i < kernel_dim; i++)
	{
		free (kernel[i]);
	}
	free (kernel);

	printf("process %d of %d done.\n", me+1, nproc);

	MPI_Finalize();
	return 0;
}


