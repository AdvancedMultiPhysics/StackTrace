#include <iostream>

#include "Utilities.h"
#include "StackTrace.h"
#include "UnitTest.h"
#include <mpi.h>


// Function to return the call stack
std::vector<StackTrace::stack_info> get_call_stack()
{
    return StackTrace::getCallStack();
}


int main( int argc, char **argv )
{

    // Initialize MPI
    MPI_Init( &argc, &argv );
    int rank, size;
    MPI_Comm_size( MPI_COMM_WORLD, &size );
    MPI_Comm_rank( MPI_COMM_WORLD, &rank );
    MPI_Barrier( MPI_COMM_WORLD );

    // Set the error handler
    Utilities::setAbortBehavior( true, true, true );
    Utilities::setErrorHandlers();

    UnitTest ut;

    // Test getting the current call stack
    auto call_stack = get_call_stack();
    if ( rank == 0 ) {
        std::cout << "\nCall stack:" << std::endl;
        for ( auto &elem : call_stack )
            std::cout << "   " << elem.print() << std::endl;
    }
    if ( !call_stack.empty() ) {
        ut.passes( "non empty call stack" );
        bool pass = false;
        if ( call_stack.size() > 1 ) {
            if ( call_stack[1].print().find( "get_call_stack()" ) != std::string::npos )
                pass = true;
        }
        if ( pass )
            ut.passes( "call stack decoded function symbols" );
        else
            ut.expected_failure( "call stack was unable to decode function symbols" );
    } else {
        ut.failure( "non empty call stack" );
    }

    // Test getting the symbols
    std::vector<void *> address;
    std::vector<char> type;
    std::vector<std::string> obj;
    int rtn = StackTrace::getSymbols( address, type, obj );
    if ( rtn == 0 && !address.empty() )
        ut.passes( "Read symbols from executable" );

    // Test getting the executable
    std::string exe = StackTrace::getExecutable();
    if ( rank == 0 )
        std::cout << "\nExecutable: " << exe << std::endl;
    if ( exe.find( "TestStack" ) != std::string::npos )
        ut.passes( "getExecutable" );
    else
        ut.failure( "getExecutable" );

    ut.report();
    int N_errors = ut.NumFailGlobal();
    MPI_Finalize();
    return N_errors;
}
