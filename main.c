#include<mpi.h>
#include<stdio.h>
#include<stdlib.h>
#include<string.h>

#define SMOOTH 0;
#define BLUR 1;
#define SHARPEN 2;
#define MEAN 3;
#define EMBOSS 4;

typedef struct {
	int type;
	int width;
	int height;
	int maxval;
	unsigned char** image;
}image;

void readInput(const char * fileName, image *img) {
	FILE *in;
	in = fopen(fileName, "rb");
	char  type[3];

	//Reading image structure
	fgets(type, 4, in);
	if (strcmp(type, "P5\n") == 0) {
		img->type = 5;
	}
	else {
		img->type = 6;
	}

	char buffer[255];
	fgets(buffer, 255, in);
	img->width = atoi(buffer);
	strcpy(buffer, strchr(buffer, ' '));
	img->height = atoi(buffer);

	fgets(buffer, 255, in);
	img->maxval = atoi(buffer);

	//Reading the image pixels
	unsigned char** image;
	if (img->type == 5) {
		//P5
		image = malloc(img->height * sizeof(char*));
		for (int i = 0; i < img->height; i++) {
			image[i] = malloc(img->width * sizeof(char));
			fread(image[i], 1, img->width, in);
		}
	}
	else {
		//P6
		image = malloc(img->height * sizeof(char*));
		for (int i = 0; i < img->height; i++) {
			image[i] = malloc(3 * img->width * sizeof(char));
			fread(image[i], 3, img->width, in);
		}
	}
	img->image = image;
	fclose(in);
}

void writeData(const char * fileName, image *img) {
	FILE *out;
	out = fopen(fileName, "wb");

	if (img->type == 6) {
		fwrite("P6\n", 3, sizeof(char), out);
	}
	else {
		fwrite("P5\n", 3, sizeof(char), out);
	}

	fprintf(out, "%d %d\n", img->width, img->height);
	fprintf(out, "%d\n", img->maxval);

	//If we have RGB pixels or only 1 pixel
	int times = 0;
	if (img->type == 5) {
		times = 1;
	}
	else {
		times = 3;
	}

	for (int i = 0; i < img->height; i++) {
		fwrite(img->image[i], 1, times * img->width, out);
		free(img->image[i]);
	}
	free(img->image);
	fclose(out);
}

int main(int argc, char * argv[])
{
	//MPI
	int rank;
	int nProcesses;
	MPI_Init(&argc, &argv);
	MPI_Comm_rank(MPI_COMM_WORLD, &rank);
	MPI_Comm_size(MPI_COMM_WORLD, &nProcesses);
	
	//============ Main FILE ==============
	//agrv[1] input
	//argv[2] output
	//argv[3 ....] Filters
	//======================================

	//Filter's Matrix Declaration
	float constant;

	//Smooth Matrix
	constant = 9.0f;
	float smooth[3][3] = { 
		{(float) 1.0f / (float) constant, (float) 1.0f / (float) constant, (float) 1.0f / (float) constant},
		{(float) 1.0f / (float) constant, (float) 1.0f / (float) constant, (float) 1.0f / (float) constant},
		{(float) 1.0f / (float) constant, (float) 1.0f / (float) constant, (float) 1.0f / (float) constant}
	};

	//Blur Matrix
	constant = 16.0f;
	float blur[3][3] = { 
		{(float) 1.0f / (float) constant, (float) 2.0f / (float) constant, (float) 1.0f / (float) constant},
		{(float) 2.0f / (float) constant, (float) 4.0f / (float) constant, (float) 2.0f / (float) constant},
		{(float) 1.0f / (float) constant, (float) 2.0f / (float) constant, (float) 1.0f / (float) constant}
	};

	//Sharpen Matrix
	constant = 3.0f;
	float sharpen[3][3] = { 
		{(float) 0.0f, (float)-2.0f / (float) constant, (float) 0.0f},
		{(float) -2.0f / (float) constant, (float) 11.0f / (float) constant, (float) -2.0f / (float) constant},
		{(float) 0.0f, (float) -2.0f / (float) constant, (float)  0.0f}
	};
	
	//Mean Matrix
	float mean[3][3] = { {-1.0f, -1.0f, -1.0f}, {-1.0f, 9.0f, -1.0f},{-1.0f, -1.0f, -1.0f} };

	//Emboss Matrix
	float emboss[3][3] = { {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, -1.0f, 0.0f} };

	//Variables
	image image;
	int filters[255];
	int numberOfFilters;
	//==============================

	if (rank == 0) {
		//Rank 0
		if (argc < 3) {
			exit(-1);
		}
		readInput(argv[1], &image);

		//Passing image details
		for (int i = 1; i < nProcesses; i++) {
			MPI_Send(&image.type, 1, MPI_INT, i, 0, MPI_COMM_WORLD);
			MPI_Send(&image.width, 1, MPI_INT, i, 0, MPI_COMM_WORLD);
			MPI_Send(&image.maxval, 1, MPI_INT, i, 0, MPI_COMM_WORLD);
		}
		
		//Passing number of filters
		numberOfFilters = argc - 3;
		
		for (int i = 0; i < numberOfFilters; i++) {
			if (strcmp(argv[3 + i], "smooth") == 0) {
				filters[i] = SMOOTH;
			}
			if (strcmp(argv[3 + i], "blur") == 0) {
				filters[i] = BLUR;
			}
			if (strcmp(argv[3 + i], "sharpen") == 0) {
				filters[i] = SHARPEN;
			}
			if (strcmp(argv[3 + i], "mean") == 0) {
				filters[i] = MEAN;
			}
			if (strcmp(argv[3 + i], "emboss") == 0) {
				filters[i] = EMBOSS;
			}
		}

		for (int i = 1; i < nProcesses; i++) {
			MPI_Send(&numberOfFilters, 1, MPI_INT, i, 0, MPI_COMM_WORLD);
		}
		//Passing filters
		for (int i = 1; i < nProcesses; i++) {
			MPI_Send(&filters, numberOfFilters, MPI_INT, i, 0, MPI_COMM_WORLD);
		}

		//Finding how many lines each process have & sending it
		int lines = image.height / nProcesses;
		for (int i = 1; i < nProcesses; i++) {
			if (i == nProcesses - 1) {
				lines = lines + image.height % nProcesses;
			}
			MPI_Send(&lines, 1, MPI_INT, i, 0, MPI_COMM_WORLD);
		}

		int times;
		if (image.type == 5) {
			times = 1;
		}
		else {
			times = 3;
		}

		//Spliting image for each process
		lines = image.height / nProcesses;
		for (int process = 1; process < nProcesses; process++) {
			int start = process * lines;
			int end;
			if (process != nProcesses - 1) {
				end = start + lines;
			}
			else {
				end = image.height;
			}
			for (int i = start; i < end; i++) {
				MPI_Send(image.image[i], times * image.width, MPI_CHAR, process, 0, MPI_COMM_WORLD);
			}
		}
	}

	//Receiving & Getting started
	int myLines;
	if (rank == 0) {
		myLines = image.height / nProcesses;
		if (nProcesses != 1) {
			myLines += 1;
		}
	}
	else {
		//Getting image details
		MPI_Recv(&image.type, 1, MPI_INT, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
		MPI_Recv(&image.width, 1, MPI_INT, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
		MPI_Recv(&image.maxval, 1, MPI_INT, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

		//Getting number of filters
		MPI_Recv(&numberOfFilters, 1, MPI_INT, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

		//Getting filters
		MPI_Recv(&filters, numberOfFilters, MPI_INT, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

		//Getting number of lines I have to change
		MPI_Recv(&myLines, 1, MPI_INT, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
		if (rank != nProcesses - 1) {
			myLines += 2;
		}
		else {
			myLines += 1;
		}
			
		//Getting the rows I have to work on
		if (image.type == 5) {
			//P5
			image.image = malloc(myLines * sizeof(unsigned char*));
			for (int i = 0; i < myLines; i++) {
				image.image[i] = malloc(image.width * sizeof(unsigned char));
				if (i != 0) {
					if (i == myLines - 1) {
						if (rank == nProcesses - 1) {
							MPI_Recv(image.image[i], image.width, MPI_UNSIGNED_CHAR, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
						}
					}
					else {
						MPI_Recv(image.image[i], image.width, MPI_UNSIGNED_CHAR, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
					}
				}
			}
		}
		else {
			//P6
			image.image = malloc(myLines * sizeof(unsigned char*));
			for (int i = 0; i < myLines; i++) {
				image.image[i] = malloc(3 * image.width * sizeof(unsigned char));
				if (i != 0) {
					if (i == myLines - 1) {
						if (rank == nProcesses - 1) {
							MPI_Recv(image.image[i], 3 * image.width, MPI_UNSIGNED_CHAR, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
						}
					}
					else {
						MPI_Recv(image.image[i], 3 * image.width, MPI_UNSIGNED_CHAR, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
					}
					
				}
			}
		}
	}
	
	//Transformation process

	//P5 or P6 file
	int times;
	if (image.type == 5) {
		times = 1;
	}
	else {
		times = 3;
	}

	//Auxiliary matrix for image processing
	unsigned char **auxImage;
	auxImage = malloc(myLines * sizeof(char*));
	for (int i = 0; i < myLines; i++) {
		auxImage[i] = malloc(times * image.width * sizeof(char));
	}
	
	for (int i = 0; i < numberOfFilters; i++) {
		if (nProcesses != 1) {
			//Sending common line to neighbours
			if (rank == 0) {
				MPI_Send(image.image[myLines - 2], times * image.width, MPI_UNSIGNED_CHAR, 1, 0, MPI_COMM_WORLD);
				MPI_Recv(image.image[myLines - 1], times * image.width, MPI_UNSIGNED_CHAR, 1, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
			}
			if (rank == nProcesses - 1) {
				if (rank % 2 == 0) {
					MPI_Send(image.image[1], times * image.width, MPI_UNSIGNED_CHAR, (rank - 1), 0, MPI_COMM_WORLD);
					MPI_Recv(image.image[0], times * image.width, MPI_UNSIGNED_CHAR, (rank - 1), 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
				}
				else {
					MPI_Recv(image.image[0], times * image.width, MPI_UNSIGNED_CHAR, (rank - 1), 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
					MPI_Send(image.image[1], times * image.width, MPI_UNSIGNED_CHAR, (rank - 1), 0, MPI_COMM_WORLD);	
				}
			}

			if (rank != 0 && rank != nProcesses - 1) {
				if (rank % 2 == 0) {
					MPI_Send(image.image[1], times * image.width, MPI_UNSIGNED_CHAR, (rank - 1), 0, MPI_COMM_WORLD);
					MPI_Send(image.image[myLines - 2], times * image.width, MPI_UNSIGNED_CHAR, (rank + 1), 0, MPI_COMM_WORLD);

					MPI_Recv(image.image[0], times * image.width, MPI_UNSIGNED_CHAR, (rank - 1), 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
					MPI_Recv(image.image[myLines - 1], times * image.width, MPI_UNSIGNED_CHAR, (rank + 1), 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
				}
				else {
					MPI_Recv(image.image[0], times * image.width, MPI_UNSIGNED_CHAR, (rank - 1), 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
					MPI_Recv(image.image[myLines - 1], times * image.width, MPI_UNSIGNED_CHAR, (rank + 1), 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

					MPI_Send(image.image[1], times * image.width, MPI_UNSIGNED_CHAR, (rank - 1), 0, MPI_COMM_WORLD);
					MPI_Send(image.image[myLines - 2], times * image.width, MPI_UNSIGNED_CHAR, (rank + 1), 0, MPI_COMM_WORLD);
				}
			}
		}
		//Applying filters
		for (int t = 0; t < times; t++) {
			for (int row = 1; row < myLines - 1; row++) {
				for (int column = times + t; column < times * image.width - 1; column += times) {
					unsigned char tempPixel = 0;

					switch (filters[i]) {
					case 0: /*SMOOTH*/
						tempPixel = image.image[row - 1][column - times] * smooth[0][0] +
							image.image[row - 1][column] * smooth[0][1] +
							image.image[row - 1][column + times] * smooth[0][2] +
							image.image[row][column - times] * smooth[1][0] +
							image.image[row][column] * smooth[1][1] +
							image.image[row][column + times] * smooth[1][2] +
							image.image[row + 1][column - times] * smooth[2][0] +
							image.image[row + 1][column] * smooth[2][1] +
							image.image[row + 1][column + times] * smooth[2][2];
						break;
					case 1:/*BLUR*/
						tempPixel = image.image[row - 1][column - times] * blur[0][0] +
							image.image[row - 1][column] * blur[0][1] +
							image.image[row - 1][column + times] * blur[0][2] +
							image.image[row][column - times] * blur[1][0] +
							image.image[row][column] * blur[1][1] +
							image.image[row][column + times] * blur[1][2] +
							image.image[row + 1][column - times] * blur[2][0] +
							image.image[row + 1][column] * blur[2][1] +
							image.image[row + 1][column + times] * blur[2][2];
						break;
					case 2:/*SHARPEN*/
						tempPixel = image.image[row - 1][column - times] * sharpen[0][0] +
							image.image[row - 1][column] * sharpen[0][1] +
							image.image[row - 1][column + times] * sharpen[0][2] +
							image.image[row][column - times] * sharpen[1][0] +
							image.image[row][column] * sharpen[1][1] +
							image.image[row][column + times] * sharpen[1][2] +
							image.image[row + 1][column - times] * sharpen[2][0] +
							image.image[row + 1][column] * sharpen[2][1] +
							image.image[row + 1][column + times] * sharpen[2][2];
						break;
					case 3:/*MEAN*/
						tempPixel = image.image[row - 1][column - times] * mean[0][0] +
							image.image[row - 1][column] * mean[0][1] +
							image.image[row - 1][column + times] * mean[0][2] +
							image.image[row][column - times] * mean[1][0] +
							image.image[row][column] * mean[1][1] +
							image.image[row][column + times] * mean[1][2] +
							image.image[row + 1][column - times] * mean[2][0] +
							image.image[row + 1][column] * mean[2][1] +
							image.image[row + 1][column + times] * mean[2][2];
						break;
					case 4:/*EMBOSS*/
						tempPixel = image.image[row - 1][column - times] * emboss[0][0] +
							image.image[row - 1][column] * emboss[0][1] +
							image.image[row - 1][column + times] * emboss[0][2] +
							image.image[row][column - times] * emboss[1][0] +
							image.image[row][column] * emboss[1][1] +
							image.image[row][column + times] * emboss[1][2] +
							image.image[row + 1][column - times] * emboss[2][0] +
							image.image[row + 1][column] * emboss[2][1] +
							image.image[row + 1][column + times] * emboss[2][2];
						break;
					}
					auxImage[row][column] = tempPixel;
				}
			}
		}
		//Overriting image with the new one
		for (int i = 1; i < myLines - 1; i++) {
			for (int j = times; j < (times * image.width) - times; j++) {
				image.image[i][j] = auxImage[i][j];
			}
		}
	}

	//Sending to Rank 0 all the data
	if (nProcesses != 1) {
		if (rank == 0) {
			myLines -= 1;
			int lineNumber = myLines;
			int process = 1;
			while (lineNumber < image.height - 1) {
				MPI_Recv(image.image[lineNumber], times * image.width, MPI_UNSIGNED_CHAR, process, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
				lineNumber++;
				if (process != nProcesses - 1) {
					if (lineNumber == myLines * (process + 1)) {
						process++;
					}
				}
			}
		}
		else {
			for (int i = 1; i < myLines - 1; i++) {
				MPI_Send(image.image[i], times*image.width, MPI_UNSIGNED_CHAR, 0, 0, MPI_COMM_WORLD);
			}
		}
	}

	//Writing the new image
	if (rank == 0) {
		writeData(argv[2], &image);
	}

	//End of the Program
	MPI_Finalize();
	return 0;
}