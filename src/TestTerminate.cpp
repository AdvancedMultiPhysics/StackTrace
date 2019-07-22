#include "StackTrace/StackTrace.h"
#include "StackTrace/Utilities.h"

#include <csignal>
#include <cstring>


// The main function
int main( int argc, char *argv[] )
{
    if ( argc != 2 ) {
        std::cout << "Incorrect number of arguments\n";
        return 0;
    }

    // Startup
    StackTrace::Utilities::setAbortBehavior( true, 2 );
    StackTrace::Utilities::setErrorHandlers();

    // Throw a signal
    if ( strcmp( argv[1], "signal" ) == 0 )
        std::raise( 6 );
    else if ( strcmp( argv[1], "abort" ) == 0 )
        StackTrace::Utilities::abort( "StackTrace::Utilities::abort", __FILE__, __LINE__ );
    else if ( strcmp( argv[1], "throw" ) == 0 )
        throw std::logic_error( "test throw" );
    else
        std::cerr << "Unknown argument\n";

    // Shutdown
    StackTrace::Utilities::clearErrorHandlers();
    StackTrace::clearSignals();
    StackTrace::clearSymbols();
    std::cout << "Finished\n";
    return 0;
}
