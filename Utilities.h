#ifndef included_StackTrace_Utilities
#define included_StackTrace_Utilities

#include <stdexcept>
#include <string>
#include <typeinfo>

#include "StackTrace/StackTrace.h"


namespace StackTrace::Utilities {


/*!
 * Aborts the run after printing an error message with file and
 * line number information.
 */
[[noreturn]] void abort( const std::string &message, const source_location &source );


/*!
 * Set the behavior of abort
 * @param throwException    Throw an exception instead of MPI_Abort (default is false)
 * @param stackType         Type of stack to get (1: thread local stack, 2: all threads, 3: global)
 */
void setAbortBehavior( bool throwException, int stackType = 2 );


//! Function to terminate the application
[[noreturn]] void terminate( const StackTrace::abort_error &err );


//! Function to set the error handlers
void setErrorHandlers( std::function<void( StackTrace::abort_error & )> abort = nullptr );


//! Function to clear the error handlers
void clearErrorHandlers();


/*!
 * Set an environmental variable
 * @param name              The name of the environmental variable
 * @param value             The value to set
 */
void setenv( const char *name, const char *value );


/*!
 * Get an environmental variable
 * @param name              The name of the environmental variable
 * @return                  The value of the enviornmental variable
 */
std::string getenv( const char *name );


/*!
 * Function to get the memory availible.
 * This function will return the total memory availible
 * Note: depending on the implimentation, this number may be rounded to
 * to a multiple of the page size.
 * If this function fails, it will return 0.
 */
size_t getSystemMemory();


/*!
 * Function to get the memory usage.
 * This function will return the total memory used by the application.
 * Note: depending on the implimentation, this number may be rounded to
 * to a multiple of the page size.
 * If this function fails, it will return 0.
 */
size_t getMemoryUsage();


/*!
 * @brief  Function to get the time
 * @details  This function will return the current time in seconds.
 *     This is referenced with respect to a point during startup and
 *     is not consistent across multiple ranks.
 */
double time();


//! Function to get the resolution of time
[[deprecated( "tick is deprecated and will be removed!!!" )]] double tick();


/*!
 * Sleep for X ms
 * @param N         Time to sleep (ms)
 */
void sleep_ms( int N );


/*!
 * Sleep for X s
 * @param N         Time to sleep (s)
 */
void sleep_s( int N );


//! Cause a segfault
void cause_segfault();


//! Return true if we are running within valgrind
bool running_valgrind();


/*!
 * @brief  Call system command
 * @details  This function calls a system command, waits for the program
 *   to execute, captures and returns the output and exit code.
 * @param[in] cmd           Command to execute
 * @param[out] exit_code    Exit code returned from child process
 * @return                  Returns string containing the output
 */
std::string exec( const std::string &cmd, int &exit_code );

/*!
 * @brief  Call system command
 * @details  This function calls a system command, waits for the program
 *   to execute, captures, and processes the output.
 *   This version is different from the previous version in that it takes a
 *   user-supplied function to process the output one line at a time.
 *   This will also avoid internal dynamic memory allocations.
 * @param[in] cmd           Command to execute
 * @param[in] fun           Function to process the output one line at a time
 * @return                  Returns exit code
 */
template<class FUNCTION>
int exec2( const char *cmd, FUNCTION &fun );


//! Return the hopefully demangled name of the given type
std::string getTypeName( const std::type_info &id );


//! Return the hopefully demangled name of the given type
template<class TYPE>
inline std::string getTypeName()
{
    return getTypeName( typeid( TYPE ) );
}


//! Enum for the operating system
enum class OS { macOS, Linux, Windows, Unknown };


//! Return the OS
constexpr OS getOS()
{
#if defined( WIN32 ) || defined( _WIN32 ) || defined( WIN64 ) || defined( _WIN64 )
    return OS::Windows;
#elif defined( __APPLE__ )
    return OS::macOS;
#elif defined( __linux ) || defined( __linux__ ) || defined( __unix ) || defined( __posix )
    return OS::Linux;
#else
    return OS::Unknown;
#endif
}


} // namespace StackTrace::Utilities

#endif
