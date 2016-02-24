#ifndef included_Utilities
#define included_Utilities


#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <vector>


namespace Utilities {

/*!
 * Aborts the run after printing an error message with file and
 * linenumber information.
 */
void abort( const std::string &message, const std::string &filename, const int line );


/*!
 * Set the behavior of abort
 * @param printMemory       Print the current memory usage (default is true)
 * @param printStack        Print the current call stack (default is true)
 * @param throwException    Throw an exception instead of MPI_Abort (default is false)
 */
void setAbortBehavior( bool printMemory, bool printStack, bool throwException );


//! Function to set the error handlers
void setErrorHandlers();


/*!
 * Set an environmental variable
 * @param printMemory       Print the current memory usage (default is true)
 * @param printStack        Print the current call stack (default is true)
 * @param throwException    Throw an exception instead of MPI_Abort (default is false)
 */
void setenv( const char *name, const char *value );


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


//! Function to get an arbitrary point in time
double time();


//! Function to get the resolution of time
double tick();


/*!
 * Sleep for X ms
 * @param milliseconds      Time to sleep (ms)
 */
void sleep_ms( int milliseconds );


/*!
 * Sleep for X s
 * @param seconds           Time to sleep (s)
 */
void sleep_s( int seconds );


//! Cause a segfault
void cause_segfault();


} // namespace Utilities


#include "UtilityMacros.h"


#endif
