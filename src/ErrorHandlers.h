#ifndef included_StackTraceErrorHandlers
#define included_StackTraceErrorHandlers

// clang-format off

#include "StackTrace/StackTrace.h"

#include <functional>

#if @SET_USE_MPI@
#include "mpi.h"
#endif

namespace StackTrace {


/*!
 * Set the error handler
 * @param[in] abort     Function to terminate the program: abort(msg,type)
 */
void setErrorHandler( std::function<void( StackTrace::abort_error& )> abort );

//! Clear the error handler
void clearErrorHandler();


#if @SET_USE_MPI@

    //! Set an error handler for MPI
    void setMPIErrorHandler( MPI_Comm comm );

    //! Clear an error handler for MPI
    void clearMPIErrorHandler( MPI_Comm comm );

    //! Initialize globalCallStack functionallity
    void globalCallStackInitialize( MPI_Comm comm );

    //! Clean up globalCallStack functionallity
    void globalCallStackFinalize();

#else
    template<class COMM> inline void setMPIErrorHandler( COMM ) {}
    template<class COMM> inline void clearMPIErrorHandler( COMM ) {}
    template<class COMM> inline void globalCallStackInitialize( COMM ) {}
    inline void globalCallStackFinalize() {}
#endif


} // namespace StackTrace

// clang-format on

#endif
