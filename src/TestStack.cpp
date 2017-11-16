#include <cmath>
#include <csignal>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <iostream>
#include <stdexcept>
#include <sys/stat.h>
#include <vector>


#include "StackTrace.h"
#include "UnitTest.h"
#include "Utilities.h"


#ifdef USE_MPI
#include "mpi.h"
int getRank()
{
    int rank = 0;
    MPI_Comm_rank( MPI_COMM_WORLD, &rank );
    return rank;
}
void barrier() { MPI_Barrier( MPI_COMM_WORLD ); }
#else
#define MPI_COMM_WORLD 0
#define MPI_THREAD_MULTIPLE 4
void MPI_Init_thread( int *argc, char **argv[], int, int * )
{
    NULL_USE( argc );
    NULL_USE( argv );
}
void MPI_Finalize() {}
void barrier() {}
int getRank() { return 0; };
#endif


// Detect the OS (defines which tests we allow to fail)
#if defined( WIN32 ) || defined( _WIN32 ) || defined( WIN64 ) || defined( _WIN64 ) || \
    defined( _MSC_VER )
#define USE_WINDOWS
#elif defined( __APPLE__ )
#define USE_MAC
#elif defined( __linux ) || defined( __linux__ ) || defined( __unix ) || defined( __posix )
#define USE_LINUX
#else
#error Unknown OS
#endif


#define pout std::cout


#define to_ms( x ) std::chrono::duration_cast<std::chrono::milliseconds>( x ).count()


// Function to return the call stack
std::vector<StackTrace::stack_info> get_call_stack( )
{
    auto stack = StackTrace::getCallStack();
    // Trick compiler to skip inline for this function with fake recursion
    if ( stack.size() > 10000 ) {
        stack = get_call_stack();
    }
    return stack;
}


// Sleep for the given time
// Note: since we are testing interrupts, we may not sleep for the desired time
//   so we need to perform the sleep in a loop
void sleep_ms( int N )
{
    auto t1 = std::chrono::high_resolution_clock::now();
    auto t2 = std::chrono::high_resolution_clock::now();
    while ( to_ms( t2 - t1 ) < N ) {
        int N2 = N - to_ms( t2 - t1 );
        std::this_thread::sleep_for( std::chrono::milliseconds( N2 ) );
        t2 = std::chrono::high_resolution_clock::now();
    }
}
void sleep_s( int N )
{
    auto t1 = std::chrono::high_resolution_clock::now();
    auto t2 = std::chrono::high_resolution_clock::now();
    while ( to_ms( t2 - t1 ) < 1000 * N ) {
        int N2 = 1000 * N - to_ms( t2 - t1 );
        std::this_thread::sleep_for( std::chrono::milliseconds( N2 ) );
        t2 = std::chrono::high_resolution_clock::now();
    }
}


// Function to perform various signal tests
int global_signal_helper[1024] = { 0 };
void handleSignal( int s )
{
    global_signal_helper[s] = 1;
    // pout << "caught " << s << std::endl;
}
void testSignal( UnitTest &ut )
{
    int rank = getRank();
    if ( rank == 0 ) {
        // Identify the signals
        pout << "\nIdentifying signals\n";
        for ( int i = 1; i <= 64; i++ )
            pout << "  " << i << ": " << StackTrace::signalName( i ) << std::endl;
        // Test setting/catching different signals
        bool pass    = true;
        auto signals = StackTrace::allSignalsToCatch();
        StackTrace::setSignals( signals, handleSignal );
        for ( auto sig : signals ) {
            raise( sig );
            sleep_ms( 10 );
            signal( sig, SIG_DFL );
            if ( global_signal_helper[sig] != 1 )
                pass = false;
        }
        pout << std::endl;
        if ( pass )
            ut.passes( "Signals" );
        else
            ut.failure( "Signals" );
    }
}


// Test current stack trace
void testCurrentStack( UnitTest &ut, bool &decoded_symbols )
{
    barrier();
    const int rank  = getRank();
    double ts1      = Utilities::time();
    auto call_stack = get_call_stack();
    double ts2      = Utilities::time();
    if ( rank == 0 ) {
        pout << "Call stack:" << std::endl;
        for ( auto &elem : call_stack )
            pout << "   " << elem.print() << std::endl;
        pout << "Time to get call stack: " << ts2 - ts1 << std::endl;
    }
    decoded_symbols = false;
    if ( !call_stack.empty() ) {
        ut.passes( "non empty call stack" );
        for ( auto &i : call_stack ) {
            if ( i.print().find( "get_call_stack" ) != std::string::npos )
                decoded_symbols = true;
        }
        if ( decoded_symbols )
            ut.passes( "call stack decoded function symbols" );
        else
            ut.expected_failure( "call stack was unable to decode function symbols" );
    } else {
        ut.failure( "non empty call stack" );
    }
    if ( rank == 0 ) {
        ts1        = Utilities::time();
        auto trace = StackTrace::backtrace();
        ts2        = Utilities::time();
        pout << "Time to get backtrace: " << ts2 - ts1 << std::endl << std::endl;
    }
}


// Test stack trace of another thread
void testThreadStack( UnitTest &ut, bool decoded_symbols )
{
    barrier();
    const int rank = getRank();
    double t1      = Utilities::time();
    std::thread thread( sleep_ms, 1000 );
    sleep_ms( 50 ); // Give thread time to start
    double t2       = Utilities::time();
    auto call_stack = StackTrace::getCallStack( thread.native_handle() );
    double t3       = Utilities::time();
    thread.join();
    double t4 = Utilities::time();
    if ( rank == 0 ) {
        pout << "Call stack (thread):" << std::endl;
        for ( auto &elem : call_stack )
            pout << "   " << elem.print() << std::endl;
        pout << "Time to get call stack (thread): " << t3 - t2 << std::endl;
        pout << std::endl;
    }
    if ( !call_stack.empty() ) {
        ut.passes( "non empty call stack (thread)" );
        bool pass = false;
        for ( auto &elem : call_stack ) {
            if ( elem.print().find( "sleep_ms" ) != std::string::npos )
                pass = true;
        }
        pass = pass && fabs( t4 - t1 ) > 0.9;
        if ( pass )
            ut.passes( "call stack (thread)" );
        else if ( !decoded_symbols )
            ut.expected_failure( "call stack (thread) failed to decode symbols" );
        else
            ut.failure( "call stack (thread)" );
    } else {
        ut.failure( "non empty call stack (thread)" );
    }
}


// Test stack trace of another thread
void testFullStack( UnitTest &ut )
{
    barrier();
    const int rank = getRank();
    std::thread thread1( sleep_ms, 2000 );
    std::thread thread2( sleep_ms, 2000 );
    std::thread thread3( sleep_s, 2 );
    sleep_ms( 50 ); // Give thread time to start
    double t1       = Utilities::time();
    auto call_stack = StackTrace::getAllCallStacks();
    cleanupStackTrace( call_stack );
    double t2 = Utilities::time();
    thread1.join();
    thread2.join();
    thread3.join();
    if ( rank == 0 ) {
        pout << "Call stack (all threads):" << std::endl;
        auto text = call_stack.print( "   " );
        for ( auto &line : text )
            pout << line << std::endl;
        pout << "Time to get call stack (all threads): " << t2 - t1 << std::endl;
        pout << std::endl;
    }
    NULL_USE( ut );
    /*    if ( !call_stack.empty() ) {
            ut.passes( "non empty call stack (thread)" );

            bool pass = false;
            for ( auto &elem : call_stack ) {
                if ( elem.print().find( "sleep_ms" ) != std::string::npos )

                    pass = true;
            }
            pass = pass && fabs(t4-t1)>1.9;
            if ( pass )
                ut.passes( "call stack (thread)" );

            else if ( !decoded_symbols )
                ut.expected_failure( "call stack (thread) failed to decode symbols" );
            else
                ut.failure( "call stack (thread)" );
        } else {

            ut.failure( "non empty call stack (thread)" );
        }*/
}


// Test stack trace of another thread
void testGlobalStack( UnitTest &ut, bool all, const std::basic_string<wchar_t>& = std::basic_string<wchar_t>() )
{
    barrier();
    const int rank = getRank();
    std::thread thread1( sleep_ms, 2000 );
    std::thread thread2( sleep_ms, 2000 );
    std::thread thread3( sleep_s, 2 );
    sleep_ms( 50 ); // Give threads time to start
    double t1 = Utilities::time();
    StackTrace::multi_stack_info call_stack;
    if ( rank == 0 || all )
        call_stack = StackTrace::getGlobalCallStacks();
    cleanupStackTrace( call_stack );
    double t2 = Utilities::time();
    thread1.join();
    thread2.join();
    thread3.join();
    barrier();
    if ( !all && rank != 0 )
        return;
    if ( rank == 0 && !all ) {
        pout << "Call stack (global):" << std::endl;
        auto text = call_stack.print( "   " );
        for ( auto &line : text )
            pout << line << std::endl;
        pout << "Time to get call stack (global): " << t2 - t1 << std::endl;
        pout << std::endl;
    }
    NULL_USE( ut );
}


// The main function
int main( int argc, char *argv[] )
{
    int provided_thread_support;
    MPI_Init_thread( &argc, &argv, MPI_THREAD_MULTIPLE, &provided_thread_support );
    int rank = getRank();
    Utilities::setAbortBehavior( true, true, true );
    StackTrace::globalCallStackInitialize( MPI_COMM_WORLD );
    UnitTest ut;
    barrier();

    // Limit the scope of variables
    {
        // Test getting the current call stack
        bool decoded_symbols = false;
        testCurrentStack( ut, decoded_symbols );

        // Test getting the stacktrace of another thread
        testThreadStack( ut, decoded_symbols );

        // Test getting the full stacktrace of all thread
        testFullStack( ut );

        // Test getting the global stack trace of all threads/processes
        testGlobalStack( ut, false );
        testGlobalStack( ut, true );

        // Test getting a list of all active threads
        std::thread thread1( sleep_ms, 1000 );
        std::thread thread2( sleep_ms, 1000 );
        sleep_ms( 50 ); // Give threads time to start
        auto thread_ids_test = StackTrace::activeThreads();
        std::set<std::thread::native_handle_type> thread_ids;
        thread_ids.insert( StackTrace::thisThread() );
        thread_ids.insert( thread1.native_handle() );
        thread_ids.insert( thread2.native_handle() );
        thread1.join();
        thread2.join();
        if ( thread_ids == thread_ids_test )
            ut.passes( "StackTrace::activeThreads" );
        else
            ut.expected_failure( "StackTrace::activeThreads" );

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
            pout << "\nExecutable: " << exe << std::endl;
        if ( exe.find( "TestStack" ) != std::string::npos )
            ut.passes( "getExecutable" );
        else
            ut.failure( "getExecutable" );

        // Test identifying signals
        testSignal( ut );

        // Test catching an error
        try {
            ERROR( "Test" );
            ut.failure( "Failed to catch ERROR" );
        } catch ( ... ) {
            ut.passes( "Caught ERROR" );
        }
        try {
            throw std::logic_error( "test" );
            ut.failure( "Failed to catch exception" );
        } catch ( ... ) {
            ut.passes( "Caught exception" );
        }

    }

    int N_errors = ut.NumFailGlobal();
    ut.report();
    ut.reset();
    if ( N_errors == 0 )
        pout << "All tests passed\n";
    StackTrace::globalCallStackFinalize();
    barrier();
    MPI_Finalize();
    return N_errors;
}
