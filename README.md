# Parallel_A3

##Outline

This program is to apply a Gaussian blur effect to a bmp format
image. This program uses a parallel implementation to achieve
this.

##Process
-1 Read in command line arguments
- Get bmp metadata: height, width, data offset, padding
-2 Get color data into 1D array
-3 Scatter data to processes
-4 Create bitmap from 1D array
-5 Apply gaussian blur effect
-6 Create 1D array from new bmp
-7 Gather 1D arrays
-8 Convert to new bmp
-9 Save to file