#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>



/*	*******************		Work good with "TSP.100" <=> N = 100 and MAX_EPOCH = 4500	*******************		*/



/*	*******************		Declaration of Function and data file paths		******************* */
char Data[50] = "TSP.30";
unsigned int is_in(unsigned int, unsigned int *, unsigned int);
/*  ******************************************************************************************* */


int main() {
	
	FILE *interim_results;
	interim_results = fopen("INTERMEDIATE_RESULTS.txt", "w");

	FILE *results_min_routes;
	results_min_routes = fopen("RESULTS_MIN_ROUTES.txt", "w");

	FILE *result_final_route;
	result_final_route = fopen("RESULT_FINAL_ROUTE.txt", "w");




/* 	*******************		Reading Data from the file to 2D Array --> Distance		******************* */
	FILE *f;	
	f = fopen(Data, "r");
    if (f == NULL) {
	 	printf("Error to open file %s\n\n", Data);
	 	return EXIT_FAILURE;
	}
	
	/* The first element in the file in the number of cities */
	/* If we know this number, then after we know all the dimensions of all arrays */
	unsigned int i = 0, x;
	while ( !feof(f) && (i == 0) ) { 
		fscanf(f, "%d", &(x));
		i = 1;
	}
	

	/* Since the first element is the number of cities, we write this value into a variable "N" */
	/* Note that the value "N" we will do constant, since the number of cities doesn`t change */
	const unsigned int N = x;

    unsigned int dimension = ((N - 1) * N) / 2;
    unsigned int Temp_Array[dimension];

    i = 1;
	while ( !feof(f) ) { 
		fscanf(f, "%d", &(Temp_Array[i]));
		i++;
	}
	fclose(f);


	/* Array Distance between cities is Symmetric 2D Array/Matrix with "0" on the diagonal */
	unsigned int Distance[N][N];
	unsigned int j, l = 1;

	for (i = 1; i <= N; i++) {
		for (j = 1; j <= N; j++) {
			if (i >= j) {
				Distance[i][j] = 0;
			}
			else {
				Distance[i][j] = Temp_Array[l];
				l++;
			}
		}
	}
	/* free(Temp_Array); */
	
	for (i = 2; i <= N; i++) {
		for (j = 1; j <= (i - 1); j++) {
			Distance[i][j] = Distance[j][i];
		}
	}	
/*  ******************************************************************************************* */


	
/*	*******************		Display an Array Distance		******************* */
	fprintf(interim_results, " -------   We work with File : %s   -------\n\n", Data);
	fprintf(interim_results, "Display an Array Distance between cities : \n\n");
	for (i = 1; i <= N; i++) {
		for (j = 1; j <= N; j++) {
			fprintf(interim_results, "%3d ", Distance[i][j]);
		}
		fprintf(interim_results, "\n");
	}
	fprintf(interim_results, "\n\n");	
/*  ***************************************************************************	 */



/* 	*******************		Creating 2D Array --> Closeness (Nu) Symmetric 2D Array/Matrix with "0" on the diagonal		******************* */
	float Closeness[N][N];
/*  We define a parameter for proximity "Q" */
#define Q 4
	
	for (i = 1; i <= N; i++) {
		for (j = 1; j <= N; j++) {
			if (i != j)
				Closeness[i][j] = Q / (Distance[i][j] * 1.0);
			else 
				Closeness[i][j] = 0.000;
		}
	}
/*  ****************************************************************************************************** */



/*	*******************		Display an Array Closeness		******************* */
	fprintf(interim_results, "Display an Array Closeness between cities : \n");
	for (i = 1; i <= N; i++) {
		for (j = 1; j <= N; j++) {
			fprintf(interim_results, "%.3f ", Closeness[i][j]);
		}
		fprintf(interim_results, "\n");
	}
	fprintf(interim_results, "\n\n");
/*  *************************************************************************** */

	

/* 	*******************		Creating 2D Array --> Pheromone (Tau) Symmetric 2D Array/Matrix with "0" on the diagona		******************* */
	float Pheromone[N][N];
/*	We first determine the values of pheromones for everyone by 0.200 */
#define tau_0 0.200
	
	for (i = 1; i <= N; i++) {
		for (j = 1; j <= N; j++) {
			if (i == j) 
				Pheromone[i][j] = 0.000;
			else 
				Pheromone[i][j] = tau_0;		
		}
	}	
/*  ******************************************************************************************************* */



/*	******************		Display an Array Pheromone before the start == EPOCH_0		******************* */
	fprintf(interim_results, "Display an Array Pheromone before the start :\n");
	fprintf(interim_results, "\n ------- Pheromone EPOCH 0 -------\n\n");
	for (i = 1; i <= N; i++) {
		for (j = 1; j <= N; j++) {
			fprintf(interim_results, "%.3f ", Pheromone[i][j]);
		}
		fprintf(interim_results, "\n");
	}
	fprintf(interim_results, "\n\n");	
/*  *************************************************************************** */



/* *******************		Definition parameters		******************* */
/*	Apla - power for Tau (Pheromone) */
/*	Beta - power for Nu (Closeness) */
/*	Ro - pheromone evaporation coefficient  ==>  Stay (1 - ro), with ro = 0,36  ==>  Stay 0,64 == 64 % */
/*	MAX_EPOCH - time/number of our itarations (t) */

#define alpha 2
#define beta 1
#define ro 0.36
#define MAX_EPOCH 50
/*  *********************************************************************** */



/*	*******************		Main cycle		******************* */
	/* We start with the main cycle by the number of Epochs	*/
	unsigned int t;
	unsigned int Min_Routes[MAX_EPOCH][N+2];
	for (t = 1; t <= MAX_EPOCH; t++) {
		
		fprintf(interim_results, "\n\n\n ------- EPOCH %d -------\n\n", t);
		fprintf(interim_results, "   * Routes : \n\n");
					
		unsigned int Route[N][N+2];
		/* We start the cycle for each Ant and Number of Ants == Number of cities */
		unsigned int k, s;		
		
		for (k = 1; k <= N; k++) {
			unsigned int l = 1, length = 0;
			Route[k][1] = k;
			Route[k][N+1] = k;

			float Probability[N];
			unsigned int next_city = k;
			
			for (s = 1; s < N; s++) {
			
				for (j = 1; j <= N; j++) {
					if ( !(is_in(j, Route[k], s)) )
						Probability[j] = pow(Closeness[next_city][j], beta) * pow(Pheromone[next_city][j], alpha);
					else 
						Probability[j] = 0.0;
				} 
				float sum = 0.000;
				for (j = 1; j <= N; j++) 
					sum = sum + Probability[j];
				for (j = 1; j <= N; j++) {
					if ( !(is_in(j, Route[k], s)) )
						Probability[j] = Probability[j] / (sum * 1.0);
					else 
						Probability[j] = 0.0;
				}
				

				float cursor = (rand() % 100) / 100.0;
				float storage_flag = Probability[1];
				unsigned int index = 1;
				for (j = 1; j <= N; j++) {
					if (storage_flag > cursor) {
						index = j;
						break;
					}
					storage_flag = storage_flag + Probability[j+1];
				}
				l++;
				next_city = index;
				Route[k][l] = index;
				
				length = length + Distance[Route[k][l-1]][Route[k][l]];
			}
			length = length + Distance[Route[k][N]][Route[k][N+1]];
			Route[k][N+2] = length;
			
			
			
			/* Ants routes visualization on the t-the iteration */
			fprintf(interim_results, "Ant[%2d] : ", k);
			for (j = 1; j <= N+2; j++)
				if (j == (N+2)) 
					fprintf(interim_results, "| %2d ", Route[k][j]);
				else
					fprintf(interim_results, "%2d ", Route[k][j]);
			fprintf(interim_results, "\n");
			
		}


		/* Finding Minimum Route for each EPOCH */
		unsigned int min_route = Route[1][N+2], index_min_route = 1;
		for (i = 1; i <= N; i++) {
			if (Route[i][N+2] < min_route) {
				min_route = Route[i][N+2];
				index_min_route = i;
			}
		}

		/* Creating an Array to memorise the best route for each epochs with its length */
		/* unsigned int Min_Routes[MAX_EPOCH][N+2]; */
		for (i = 1; i <= N+2; i++)
			Min_Routes[t][i] = Route[index_min_route][i];

		/* Displaying an Array with Minimum Route for each EPOCH to File "RESULTS_MIN_ROUTES.txt" */
		fprintf(results_min_routes, "EPOCH [%2d] :", t);
		for (i = 1; i <= N+2; i++)
			if (i == (N+2)) 
				fprintf(results_min_routes, "| %2d ", Min_Routes[t][i]);
			else
				fprintf(results_min_routes, "%2d ", Min_Routes[t][i]);
		fprintf(results_min_routes, "\n");
		


		
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
		
					
		/* Cycle for Pheromone if (1 --> 2) and (2 --> 1) are different
			for (s = 1; s <= N+1; s++) {
				i = Route[k][s];
				j = Route[k][s+1];
				Pheromone[i][j] = Pheromone[i][j] + ( Q / (Route[k][N+2] * 1.0) );
			}
		*/
		
		/* Cycle for Pheromone if (1 --> 2) and (2 --> 1) are the same */
		for (k = 1; k <= N; k++) {
			for (s = 1; s <= N; s++) {
				i = Route[k][s];
				j = Route[k][s+1];
				Pheromone[i][j] = Pheromone[i][j] + ( Q / (Route[k][N+2] * 1.0) );
				Pheromone[j][i] = Pheromone[i][j];
			}
		}
		
		/* Pheromone array visualization on the t-the iteration */
		fprintf(interim_results, "\n   * Pheromone : \n\n");
		for (i = 1; i <= N; i++) {
			for (j = 1; j <= N; j++) {
				fprintf(interim_results, "%.3f ", Pheromone[i][j]);
			}
			fprintf(interim_results, "\n");
		}
		fprintf(interim_results, "\n\n");	
		
		



	}


	/* Finding for the Minimum Route of the Minimums. Because for example for "TSP.30" and MAX_EPOCH = 5, Min Route is not in the EPOCH[MAX_EPOCH] */
	unsigned int min_route_final = Min_Routes[1][N+2], index_min_route_final = 1;

	for (i = 1; i <= MAX_EPOCH; i++) {
		if (min_route_final >= Min_Routes[i][N+2]) {
			min_route_final = Min_Routes[i][N+2];
			index_min_route_final = i;
		}
	}

	/* Writing the Last/"Best" Final Route */
	for (i = 1; i <= N+2; i++) {
		if (i == (N+2)) 
			fprintf(result_final_route, "| %2d ", Min_Routes[index_min_route_final][i]);
		else
			fprintf(result_final_route, "%2d ", Min_Routes[index_min_route_final][i]);
	}




	fclose(interim_results);
	fclose(results_min_routes);
	fclose(result_final_route);
	
	
	return EXIT_SUCCESS;
}
/*  ******************************************************************************************* */



/*	*******************		Definition of Function and data file paths		******************* */
unsigned int is_in(unsigned int x, unsigned int a[], unsigned int length) {
	unsigned int flag = 0, i = 1;
	
	while ((i < length) && (flag != 1)) {
		if (a[i] == x)
			flag = 1;
		i++;
	}
	
	return flag;
} 
/*  ******************************************************************************************* */