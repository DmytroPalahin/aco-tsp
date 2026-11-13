#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

/*	*******************		Declaration of Function and data file paths		******************* */
char Data[1000] = "TSP.10";
unsigned int is_in(unsigned int, unsigned int *, unsigned int);
/*  ******************************************************************************************* */

int main()
{

	/* 	*******************		Reading Data from the file to 2D Array --> Distance		******************* */
	FILE *f;
	f = fopen(Data, "r");
	if (f == NULL)
	{
		printf("Error to open file %s\n\n", Data);
		return EXIT_FAILURE;
	}

	/* Firstly, we will read data to 1D Array --> Temp_Array  and after will "cut" data	*/
	unsigned int Temp_Array[10000];
	/* We use counter "l" to move through the file */
	unsigned int l = 1;
	while (!feof(f))
	{
		fscanf(f, "%u", &(Temp_Array[l]));
		l++;
	}

	/* Since the first element is the number of cities, we write this value into a variable "N" */
	/* Note that the value "N" we will do constant, since the number of cities doesn`t change */
	/* Note that he number of cities is the first number which we have read to "Temp_Array" */
	const unsigned int N = Temp_Array[1];
	/* Array Distance between cities is Symmetric 2D Array/Matrix with "0" on the diagonal */
	unsigned int Distance[N][N];
	unsigned int i, j;
	l = 2;
	for (i = 1; i <= N; i++)
	{
		for (j = 1; j <= N; j++)
		{
			if (i >= j)
			{
				Distance[i][j] = 0;
			}
			else
			{
				Distance[i][j] = Temp_Array[l];
				l++;
			}
		}
	}

	for (i = 2; i <= N; i++)
	{
		for (j = 1; j <= (i - 1); j++)
		{
			Distance[i][j] = Distance[j][i];
		}
	}
	/*  ******************************************************************************************* */

	/*	*******************		Display an Array Distance		******************* */
	printf("Display an Array Distance between cities : \n");
	for (i = 1; i <= N; i++)
	{
		for (j = 1; j <= N; j++)
		{
			printf("%3d ", Distance[i][j]);
		}
		printf("\n");
	}
	printf("\n\n");
	/*  ***************************************************************************	*/

	/* 	*******************		Creating 2D Array --> Closeness (Nu) Symmetric 2D Array/Matrix with "0" on the diagonal		******************* */
	float Closeness[N][N];
/*  We define a parameter for proximity "Q" */
#define Q 4

	for (i = 1; i <= N; i++)
	{
		for (j = 1; j <= N; j++)
		{
			if (i != j)
				Closeness[i][j] = Q / (Distance[i][j] * 1.0);
			else
				Closeness[i][j] = 0.000;
		}
	}
	/*  ****************************************************************************************************** */

	/*	*******************		Display an Array Closeness		******************* */
	printf("Display an Array Closeness between cities : \n");
	for (i = 1; i <= N; i++)
	{
		for (j = 1; j <= N; j++)
		{
			printf("%.3f ", Closeness[i][j]);
		}
		printf("\n");
	}
	printf("\n\n");
	/*  *************************************************************************** */

	/* 	*******************		Creating 2D Array --> Pheromone (Tau) Symmetric 2D Array/Matrix with "0" on the diagona		******************* */
	float Pheromone[N][N];
/*	We first determine the values of pheromones for everyone by 0.200 */
#define tau_0 0.200

	for (i = 1; i <= N; i++)
	{
		for (j = 1; j <= N; j++)
		{
			if (i == j)
				Pheromone[i][j] = 0.000;
			else
				Pheromone[i][j] = tau_0;
		}
	}
	/*  ******************************************************************************************************* */

	/*	******************		Display an Array Pheromone before the start == EPOCH_0		******************* */
	printf("Display an Array Pheromone before the start :\n");
	printf("\n ------- Pheromone EPOCH 0 -------\n\n");
	for (i = 1; i <= N; i++)
	{
		for (j = 1; j <= N; j++)
		{
			printf("%.3f ", Pheromone[i][j]);
		}
		printf("\n");
	}
	printf("\n\n");
	/*  *************************************************************************** */

	/* *******************		Definition parameters		******************* */
	/*	Apla - power for Tau (Pheromone) */
	/*	Beta - power for Nu (Closeness) */
	/*	Ro - pheromone evaporation coefficient  ==>  Stay (1 - ro), with ro = 0,36  ==>  Stay 0,64 == 64 % */
	/*	MAX_EPOCH - time/number of our itarations (t) */

#define alpha 4.0
#define beta 1.0
#define ro 0.36
#define MAX_EPOCH 2
	/*  *********************************************************************** */

	/*	*******************		Main cycle		******************* */
	/* We start with the main cycle by the number of Epochs	*/
	unsigned int t;
	for (t = 1; t <= MAX_EPOCH; t++)
	{

		printf("\n\n\n ------- EPOCH %d -------\n\n", t);
		printf("   * Routes : \n\n");

		unsigned int Route[N][N + 2];
		/* We start the cycle for each Ant and Number of Ants == Number of cities */
		unsigned int k, s;

		for (k = 1; k <= N; k++)
		{
			unsigned int l = 1, length = 0;
			Route[k][1] = k;
			Route[k][N + 1] = k;

			float Probability[N];
			unsigned int next_city = k;

			for (s = 1; s < N; s++)
			{

				for (j = 1; j <= N; j++)
				{
					if (!(is_in(j, Route[k], s)))
						Probability[j] = pow(Closeness[next_city][j], beta) * pow(Pheromone[next_city][j], alpha);
					else
						Probability[j] = 0.0;
				}
				float sum = 0.000;
				for (j = 1; j <= N; j++)
					sum = sum + Probability[j];
				for (j = 1; j <= N; j++)
				{
					if (!(is_in(j, Route[k], s)))
						Probability[j] = Probability[j] / (sum * 1.0);
					else
						Probability[j] = 0.0;
				}

				float max = Probability[1];
				unsigned int max_index = 1;
				for (j = 1; j <= N; j++)
				{
					if (max < Probability[j])
					{
						max = Probability[j];
						max_index = j;
					}
				}
				l++;
				next_city = max_index;
				Route[k][l] = max_index;
				length = length + Distance[Route[k][l - 1]][Route[k][l]];
			}
			length = length + Distance[Route[k][N]][Route[k][N + 1]];
			Route[k][N + 2] = length;

			/* Ants routes visualization on the t-the iteration */
			printf("Ant[%2d] : ", k);
			for (j = 1; j <= N + 2; j++)
				if (j == (N + 2))
					printf("| %2d ", Route[k][j]);
				else
					printf("%2d ", Route[k][j]);
			printf("\n");
		}

		/* Pheromone Array Updates  <==> Evaporation Pheromones */
		for (i = 1; i <= N; i++)
			for (j = 1; j <= N; j++)
				if (i < j)
					Pheromone[i][j] = Pheromone[i][j] * (1 - ro);

		/* Doing matrix symmetric */
		for (i = 1; i <= N; i++)
			for (j = 1; j <= N; j++)
				if (i > j)
					Pheromone[i][j] = Pheromone[j][i];

		/*
		 Cycle for Pheromone if (1 --> 2) and (2 --> 1) are different
			for (s = 1; s <= N+1; s++) {
				i = Route[k][s];
				j = Route[k][s+1];
				Pheromone[i][j] = Pheromone[i][j] + ( Q / (Route[k][N+2] * 1.0) );
			}
		*/

		/* Cycle for Pheromone if (1 --> 2) and (2 --> 1) are the same */
		for (k = 1; k <= N; k++)
		{
			for (s = 1; s <= N; s++)
			{
				i = Route[k][s];
				j = Route[k][s + 1];
				Pheromone[i][j] = Pheromone[i][j] + (Q / (Route[k][N + 2] * 1.0));
				Pheromone[j][i] = Pheromone[i][j];
			}
		}

		/* Pheromone array visualization on the t-the iteration */
		printf("\n   * Pheromone : \n\n");
		for (i = 1; i <= N; i++)
		{
			for (j = 1; j <= N; j++)
			{
				printf("%.3f ", Pheromone[i][j]);
			}
			printf("\n");
		}
		printf("\n\n");
	}

	return EXIT_SUCCESS;
}
/*  ******************************************************************************************* */

/*	*******************		Definition of Function and data file paths		******************* */
unsigned int is_in(unsigned int x, unsigned int a[], unsigned int length)
{
	unsigned int flag = 0, i = 1;

	while ((i < length) && (flag != 1))
	{
		if (a[i] == x)
			flag = 1;
		i++;
	}

	return flag;
}
/*  ******************************************************************************************* */