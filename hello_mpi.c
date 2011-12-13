#include "mpi.h"
#include <stdio.h>
#include <string.h>

int main( int argc, char *argv[] )
{
  int  numprocs, myrank, namelen, i, catch;
    char processor_name[MPI_MAX_PROCESSOR_NAME];
    char greeting[MPI_MAX_PROCESSOR_NAME + 80];
    MPI_Status status;

    MPI_Init( &argc, &argv );
    MPI_Comm_size( MPI_COMM_WORLD, &numprocs );
    MPI_Comm_rank( MPI_COMM_WORLD, &myrank );
    MPI_Get_processor_name( processor_name, &namelen );

    sprintf( greeting, "Hello, world, from process %d of %d on %s",
             myrank, numprocs, processor_name );
	MPI_Barrier(MPI_COMM_WORLD);
    if ( myrank == 0 ) {
	  MPI_Bcast(&myrank, 1, MPI_INT, myrank, MPI_COMM_WORLD);
	  printf( "%s\n", greeting );
	  for ( i = 1; i < numprocs; i++ ) {
		MPI_Recv( greeting, sizeof( greeting ), MPI_CHAR,
				  i, 1, MPI_COMM_WORLD, &status );
		printf( "%s\n", greeting );
        }
    }
    else {
	  MPI_Bcast(&catch, 1, MPI_INT, 0, MPI_COMM_WORLD);
	  MPI_Send( greeting, sizeof( greeting ), MPI_CHAR,
				0, 1, MPI_COMM_WORLD );
    }
	MPI_Barrier(MPI_COMM_WORLD);
    MPI_Finalize( );
    return 0;
}

