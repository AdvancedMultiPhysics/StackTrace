#include <iostream>

#include "StackTrace.h"
#include "UnitTest.h"
#include "Utilities.h"

#ifdef USE_MPI
#include <mpi.h>
#endif


// Function to return the call stack
std::vector<StackTrace::stack_info> get_call_stack() { return StackTrace::getCallStack(); }


int main( int argc, char **argv )
{

    // Initialize MPI
    int rank=0, size=1;
#ifdef USE_MPI
    MPI_Init( &argc, &argv );
    MPI_Comm_size( MPI_COMM_WORLD, &size );
    MPI_Comm_rank( MPI_COMM_WORLD, &rank );
    MPI_Barrier( MPI_COMM_WORLD );
#endif

    // Set the error handler
    Utilities::setAbortBehavior( true, true, true );
    Utilities::setErrorHandlers();

    UnitTest ut;

    // Test getting the current call stack
    double ts1 = Utilities::time();
    auto call_stack = get_call_stack();
    double ts2 = Utilities::time();
    if ( rank == 0 ) {
        std::cout << "Call stack:" << std::endl;
        for ( auto &elem : call_stack )
            std::cout << "   " << elem.print() << std::endl;
        std::cout << "Time to get call stack: " << ts2-ts1 << std::endl;
    }
    if ( !call_stack.empty() ) {
        ut.passes( "non empty call stack" );
        bool pass = false;
        if ( call_stack.size() > 1 ) {
            if ( call_stack[1].print().find( "getCallStack" ) != std::string::npos )
                pass = true;
        }
        if ( pass )
            ut.passes( "call stack decoded function symbols" );
        else
            ut.expected_failure( "call stack was unable to decode function symbols" );
    } else {
        ut.failure( "non empty call stack" );
    }
    ts1 = Utilities::time();
    auto trace = StackTrace::backtrace();
    ts2 = Utilities::time();
    std::cout << "Time to get backtrace: " << ts2-ts1 << std::endl;

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

    // Test catching an error
    try {
        throw std::logic_error( "test" );
        ut.failure( "Failed to catch exception" );
    } catch ( ... ) {
        ut.passes( "Caught exception" );
    }

    ut.report();
    int N_errors = ut.NumFailGlobal();
#ifdef USE_MPI
    MPI_Finalize();
#endif
    return N_errors;
}
