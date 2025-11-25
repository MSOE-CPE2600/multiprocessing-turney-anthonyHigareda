/// 
//  CPE 2600 - 131
// 	Dr. Turney
//	Fall 2025
//  Lab 11: Multiprocessing
//	Modified by: Anthony Higareda
//
//  mandel.c
//  Based on example code found here:
//  https://users.cs.fiu.edu/~cpoellab/teaching/cop4610_fall22/project3.html
//
//  Converted to use jpg instead of BMP and other minor changes
//  
///
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <wait.h>
#include <sys/stat.h>
#include <pthread.h>
#include "jpegrw.h"
#define MAX_FILENAME_LENGTH 20
#define NUM_FRAMES 50

// local routines
static int iteration_to_color( int i, int max );
static int iterations_at_point( double x, double y, int max );
static void compute_image( imgRawImage *img, double xmin, double xmax,
									double ymin, double ymax, int max, int threads);
static void* create_img_slice(void *arg);
static void show_help();

typedef struct thread_slice
{
	imgRawImage *img;
	double thread_xmin;
	double thread_xmax;
	double ymin;
	double ymax;
	int max;
	int thread_width;
	int height;
} thread_slice;

int main( int argc, char *argv[] )
{
	char c;

	// These are the default configuration values used
	// if no command line arguments are given.	
	char  *outfile = malloc(sizeof(char) * MAX_FILENAME_LENGTH);
	double xcenter = 0;
	double ycenter = 0;
	double xscale = 4;
	double yscale = 0; // calc later
	int    image_width = 1000;
	int    image_height = 1000;
	int    max = 1000;
	int	   max_processes = 1; // default number of children
	int	   num_threads = 1; // default number of threads

	const double xtarget = 0.25970000000095;
	const double ytarget = 0.0015249999999;
	const double scaletarget = 0.0000000000001;

	double xcenters[NUM_FRAMES];
	double ycenters[NUM_FRAMES];
	double scalesteps[NUM_FRAMES];

	xcenters[0] = xcenter;
	ycenters[0] = ycenter;
	scalesteps[0] = xscale;

	for (int i = 1; i < NUM_FRAMES; i++)
	{
		// approach the final goal parameters every step
		xcenters[i] = (xcenters[i - 1] + xtarget) / 2;
		ycenters[i] = (ycenters[i - 1] + ytarget) / 2;
		scalesteps[i] = (scalesteps[i - 1] - scaletarget) / 1.8;
	}

	// For each command line argument given,
	// override the appropriate configuration value.

	while((c = getopt(argc,argv,"x:y:s:W:H:m:o:p:t:h"))!=-1) {
		switch(c) 
		{
			case 'x':
				xcenter = atof(optarg);
				break;
			case 'y':
				ycenter = atof(optarg);
				break;
			case 's':
				xscale = atof(optarg);
				break;
			case 'W':
				image_width = atoi(optarg);
				break;
			case 'H':
				image_height = atoi(optarg);
				break;
			case 'm':
				max = atoi(optarg);
				break;
			case 'o':
				strcpy(outfile, optarg);
				break;
			case 'p':
				max_processes = atoi(optarg);
				break;
			case 't':
				num_threads = atoi(optarg);
				break;
			case 'h':
				show_help();
				exit(1);
				break;
		}
	}

	int num_active_processes = 0;

	for (int i = 0; i < NUM_FRAMES; i++)
	{

		if (num_active_processes >= max_processes)
		{
			wait(NULL);
			num_active_processes--;
		}
		pid_t pid = fork();
		if (pid == 0)
		{
			sprintf(outfile, "mandel%d.jpg", i);

			// Calculate y scale based on x scale (settable) and image sizes in X and Y (settable)
			yscale = scalesteps[i] / image_width * image_height;

			// Display the configuration of the image.
			printf("mandel: x=%lf y=%lf xscale=%lf yscale=%1f max=%d outfile=%s\n",xcenters[i],ycenters[i],scalesteps[i],yscale,max,outfile);

			// Create a raw image of the appropriate size.
			imgRawImage* img = initRawImage(image_width,image_height);

			// Fill it with a black
			setImageCOLOR(img,0);

			// Compute the Mandelbrot image
			compute_image(img,xcenters[i]-scalesteps[i]/2,xcenters[i]+scalesteps[i]/2,ycenters[i]-yscale/2,ycenters[i]+yscale/2,max,num_threads);

			// Save the image in the stated file.
			storeJpegImageFile(img,outfile);

			// free the mallocs
			freeRawImage(img);
			exit(0);
		}
		else if (pid > 0)
		{
			num_active_processes++;
		}
	}

	while (num_active_processes > 0)
	{
		wait(NULL);
		num_active_processes--;
	}

	free(outfile);
	return 0;
}




/*
Return the number of iterations at point x, y
in the Mandelbrot space, up to a maximum of max.
*/

int iterations_at_point( double x, double y, int max )
{
	double x0 = x;
	double y0 = y;

	int iter = 0;

	while( (x*x + y*y <= 4) && iter < max ) {

		double xt = x*x - y*y + x0;
		double yt = 2*x*y + y0;

		x = xt;
		y = yt;

		iter++;
	}

	return iter;
}

/*
Compute an entire Mandelbrot image, writing each point to the given bitmap.
Scale the image to the range (xmin-xmax,ymin-ymax), limiting iterations to "max"
*/

void compute_image(imgRawImage* img, double xmin, double xmax, double ymin, double ymax, int max, int threads)
{
	int thread_width = img->width / threads;
	int height = img->height;

	pthread_t pthreads[threads];

	for (int t = 0; t < threads; t++)
	{
		thread_slice slice;
		slice.img = img;
		slice.thread_xmin = thread_width * t;
		slice.thread_xmax = thread_width * (t + 1);
		slice.ymin = ymin;
		slice.ymax = ymax;
		slice.max = max;
		slice.thread_width = thread_width;
		slice.height = height;

		pthread_create(&pthreads[t], NULL, &create_img_slice, (void*)&slice);
	}
	for (int t = 0; t < threads; t++)
	{
		pthread_join(pthreads[t], NULL);
	}
}

static void* create_img_slice(void* arg)
{
		// For every pixel in the image...
	thread_slice *slice = (thread_slice*)arg;

	for(int j=0;j<slice -> height;j++) {

		for(int i=0;i<slice -> thread_width;i++) {

			// Determine the point in x,y space for that pixel.
			double x = slice -> thread_xmin + i*(slice -> thread_xmax-slice -> thread_xmin)/slice -> thread_width;
			double y = slice -> ymin + j*(slice -> ymax-slice -> ymin)/slice -> height;

			// Compute the iterations at that point.
			int iters = iterations_at_point(x,y,slice -> max);

			// Set the pixel in the bitmap.
			setPixelCOLOR(slice -> img,i,j,iteration_to_color(iters,slice -> max));
		}
	}
	return NULL;
}

/*
Convert a iteration number to a color.
Here, we just scale to gray with a maximum of imax.
Modify this function to make more interesting colors.
*/
int iteration_to_color( int iters, int max )
{
	int color = 0xFEFDEF*iters/(double)max;
	return color;
}


// Show help message
void show_help()
{
	printf("Use: mandel [options]\n");
	printf("Where options are:\n");
	printf("-m <max>    The maximum number of iterations per point. (default=1000)\n");
	printf("-x <coord>  X coordinate of image center point. (default=0)\n");
	printf("-y <coord>  Y coordinate of image center point. (default=0)\n");
	printf("-s <scale>  Scale of the image in Mandlebrot coordinates (X-axis). (default=4)\n");
	printf("-W <pixels> Width of the image in pixels. (default=1000)\n");
	printf("-H <pixels> Height of the image in pixels. (default=1000)\n");
	printf("-o <file>   Set output file. (default=mandel.bmp)\n");
	printf("-p <child>  The maximum number of children processes. (default=1)\n");
	printf("-t <thread> The number of threads to use to compute the image. (default=1)\n");
	printf("-h          Show this help text.\n");
	printf("\nSome examples are:\n");
	printf("mandel -x -0.5 -y -0.5 -s 0.2\n");
	printf("mandel -x -.38 -y -.665 -s .05 -m 100\n");
	printf("mandel -x 0.286932 -y 0.014287 -s .0005 -m 1000\n\n");
}
