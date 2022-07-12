#ifndef included_StackTrace_Utilities_hpp
#define included_StackTrace_Utilities_hpp

#include "StackTrace/Utilities.h"

#include <csignal>


/****************************************************************************
 *  Utility to call system command and return output                         *
 ****************************************************************************/
#if defined( WIN32 ) || defined( _WIN32 ) || defined( WIN64 ) || defined( _WIN64 ) || \
    defined( _MSC_VER )
#define popen _popen
#define pclose _pclose
#endif
template<class FUNCTION>
int StackTrace::Utilities::exec2( const char *cmd, FUNCTION &fun )
{
    auto sig  = signal( SIGCHLD, SIG_IGN ); // Clear child exited
    auto pipe = popen( cmd, "r" );
    if ( pipe == nullptr )
        return -1;
    while ( !feof( pipe ) ) {
        char buffer[0x2000];
        buffer[0] = 0;
        auto ptr  = fgets( buffer, sizeof( buffer ), pipe );
        if ( buffer[0] != 0 && ptr == buffer )
            fun( buffer );
    }
    int code = pclose( pipe );
    if ( errno == ECHILD ) {
        errno = 0;
        code  = 0;
    }
    std::this_thread::yield(); // Allow any signals to process
    signal( SIGCHLD, sig );    // Restore previous value for child exited
    return code;
}


#endif
