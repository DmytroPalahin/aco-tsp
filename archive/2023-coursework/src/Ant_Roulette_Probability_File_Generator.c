#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>


/*	*******************		----- FUNCTIONS DECLARATION ----- *******************	*/

unsigned int is_in(unsigned int, unsigned int *, unsigned int);
int distance_beetwen_cities(int, int, int, int);

/*  ******************************************************************************************* */


int main() {

/* *******************		Definition parameters		******************* */

/*  N - Number of cities */
/*	MAX_EPOCH - time/number of our itarations (t) */

/*	Apla - power for Tau (Pheromone) */
/*	Beta - power for Nu (Closeness) */
/*	Ro - pheromone evaporation coefficient  ==>  Stay (1 - ro), with ro = 0,36  ==>  Stay 0,64 == 64 % */
/*  Q - a parameter for proximity */
/*  tau_0 - the value of pheromones at the beginning*/



#define N 50
#define MAX_EPOCH 150

#define alpha 1
#define beta 3
#define ro 0.36
#define Q 4
#define tau_0 0.200
/*  *********************************************************************** */

	int length = 150; /* length - Max length for coordinates in the axe OX and OY */
    unsigned int i, j;
    int Position_x[N], Position_y[N];

	FILE* interim_results;
	interim_results = fopen("GENERATOR_INTERMEDIATE_RESULTS.txt", "w");

	FILE* results_min_routes;
	results_min_routes = fopen("GENERATOR_RESULTS_MIN_ROUTES.txt", "w");

	FILE *result_final_route;
	result_final_route = fopen("GENERATOR_RESULT_FINAL_ROUTE.txt", "w");

	fprintf(interim_results, " -------   We work with N = %d cities   -------\n\n", N);
    fprintf(interim_results, "X[] :\n");

    srand(time(NULL));
    for (i = 1; i <= N; i++) {
        Position_x[i] = rand() % length;
        fprintf(interim_results, "%2d ", Position_x[i]);    /* Display the OX coordinates */
    }
    fprintf(interim_results, "\n");


    fprintf(interim_results, "Y[] :\n");
    for (i = 1; i <= N; i++) {
        Position_y[i] = rand() % length; 
        fprintf(interim_results, "%2d ", Position_y[i]);    /* Display the OY coordinates */
    }
    fprintf(interim_results, "\n");


	int Cord_Y[N];
	memcpy(Cord_Y, Position_y, sizeof(Cord_Y));


    /* Distance beetwen cities */
    fprintf(interim_results, "\n\nDistance beetwen cities : \n");
    int Distance[N][N]; 
    for (i = 1; i <= N; i++) {
        for (j = 1; j <= N; j++) {
            if (i == j) 
                Distance[i][j] = 0;
            else
                Distance[i][j] = distance_beetwen_cities(Position_x[i], Position_x[j], Position_y[i], Position_y[j]);
            fprintf(interim_results, "[%2d %2d]   \t->\t%2d\n", i, j, Distance[i][j]);
        }
    } 

    /* ---------------- Create MATRIX with Distance beetwen cities ---------------- */
	/* P.S. We can work also without this Array, just to have the same Data Enter */
    fprintf(interim_results, "\nCreate MATRIX with Distance beetwen cities :\n");

    FILE* f;
	f = fopen("GENERATOR_DATA_ANT.txt", "w");
    fprintf(f, "%d\n", N);
    fprintf(interim_results, "%d\n", N);

    unsigned int MATRIX_Distance[N][N];
    for (i = 1; i <= N; i++) {
        for (j = 1; j <= N; j++) {
            if (j > i) {
                MATRIX_Distance[i][j] = Distance[i][j];
                fprintf(interim_results, "%d  ", MATRIX_Distance[i][j]); 
                fprintf(f, "%d ", MATRIX_Distance[i][j]); 
            }
        } 
        fprintf(interim_results, "\n");
        fprintf(f,"\n");        
    } 
    fclose(f);


	char Data[50] = "DATA_ANT_GENERATOR.txt";
	
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
	
	for (i = 1; i <= N; i++) {
		for (j = 1; j <= N; j++) {
			if (i != j)
				Closeness[i][j] = Q / (Distance[i][j] * 1.0);
			else 
				Closeness[i][j] = 0.000;
		}
	}
/*  ************************************************************************************************************************************** */



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






/*	*******************		Main cycle		******************* */
	/* We start with the main cycle by the number of Epochs */	
	unsigned int t;

	/* Arrat Route will change for each iteration */
	/* Remark : the first and last city for the K-th Ant is the same <=> From which city we leave, to that we come*/
	/* Remark : the last column we will write the Length our Route*/
	unsigned int Route[N][N+2];
	unsigned int Min_Routes[MAX_EPOCH][N+2];
	float Probability[N];
	for (t = 1; t <= MAX_EPOCH; t++) {
		
		fprintf(interim_results, "\n\n\n ------- EPOCH %d -------\n\n", t);
		fprintf(interim_results, "   * Routes : \n\n");
					
		/*unsigned int Route[N][N+2];*/
		/* We start the cycle for each Ant and Number of Ants == Number of cities */
		unsigned int k, s;		
		
		for (k = 1; k <= N; k++) {
			unsigned int l = 1, length = 0;
			Route[k][1] = k;
			Route[k][N+1] = k;

			/*float Probability[N];*/
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
				
		 		/* srand(time(NULL)); */
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



	/* Finding for the Minimum Route of the Minimums */
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





/*	*******************		Write The Minimum Route to File for GNUPLOT "GENERATOR_ROUTE_GNUPLOT.txt"		******************* */
	FILE* f_route_gnuplot = fopen("GENERATOR_COORDINATE_GNUPLOT.txt","w");	

	unsigned int temp, Last_City_X = Position_x[N];
    for (i = 1; i <= N+1; i++) {
        temp = Min_Routes[index_min_route_final][i];
		if (temp == N)
        	fprintf(f_route_gnuplot, "%2d  %2d\n", Last_City_X, Position_y[temp]);
		else
			fprintf(f_route_gnuplot, "%2d  %2d\n", Position_x[temp], Position_y[temp]);
		
    }
    fclose(f_route_gnuplot);
/*  ************************************************************************************************************************** */	



/*	*******************		DRAW in GNUPLOT		******************* */
	FILE * pipe;
    if ((pipe = _popen("\"D:\\6_Install\\Gnuplot\\bin\\gnuplot.exe\" -persistent", "w")) == NULL) {
		printf("Error to open Gnuplot\n\n");
	 	return EXIT_FAILURE;
    }
	
	
	fprintf(pipe, "reset\n"); /* gnuplot commands */
  	fprintf(pipe, "set xlabel 'Position X'\n"); 
  	fprintf(pipe, "set ylabel 'Position Y'\n"); 
  	fprintf(pipe, "set yrange [0:length+5]\n"); 
  	fprintf(pipe, "set xrange [-1:length+5]\n"); 
  	fprintf(pipe, "set grid xtics ytics\n"); 
  	fprintf(pipe, "set title 'ANT Route' font 'Helvetica Bold, 20'\n"); 
    fprintf(pipe, "plot 'C:\\Users\\Admin\\Desktop\\Sup_Galilee\\2_anne\\S_2\\4_INFORMATIQUE\\!_Project_C_Ant\\GENERATOR_COORDINATE_GNUPLOT.txt' with linespoints pt 5  lt rgb 'red' \n");
    fflush(pipe);
    getchar();
    fprintf(pipe, "exit\n");
    fflush(pipe);
    fclose(pipe);
/*  *********************************************************************** */
  
  	


	return EXIT_SUCCESS;
}
/*  ********************************************************************************************************************************************************************************************* */



/*	*******************		----- FUNCTIONS DEFINITION ----- *******************	*/

unsigned int is_in(unsigned int x, unsigned int a[], unsigned int length) {
	unsigned int flag = 0, i = 1;
	
	while ((i < length) && (flag != 1)) {
		if (a[i] == x)
			flag = 1;
		i++;
	}
	
	return flag;
}


int distance_beetwen_cities(int x1, int x2, int y1, int y2) {
    float distance_f;
    distance_f = sqrt(pow(x2 - x1, 2) + pow(y2 - y1, 2));

    if ((distance_f - (int)distance_f) < 0.5 )
        return (int)distance_f;
    else
        return ((int)distance_f + 1);
}
/*  ******************************************************************************* */