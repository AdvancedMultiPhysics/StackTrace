#include <iostream>

#include "Utilities.h"
#include <mpi.h>


void test_segfault( int i )
{
    if ( i > 0 )
        test_segfault( i - 1 );
    else
        Utilities::cause_segfault();
}


int main( int argc, char **argv )
{
    // Initialize MPI
    MPI_Init( &argc, &argv );
    int rank, size;
    MPI_Comm_size( MPI_COMM_WORLD, &size );
    MPI_Comm_rank( MPI_COMM_WORLD, &rank );
    printf( "Rank %i of %i\n", rank, size );
    MPI_Barrier( MPI_COMM_WORLD );
    // Set the error handler
    Utilities::setAbortBehavior( true, true, true );
    Utilities::setErrorHandlers();
    // Cause a segfault
    test_segfault( 4 );
    // Finish (should not actually occur)
    std::cout << "You should never see this message" << std::endl;
    MPI_Finalize();
}
