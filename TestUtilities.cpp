#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <memory>
#include <random>
#include <sstream>
#include <vector>

#include "StackTrace/Utilities.h"


using namespace StackTrace;


// NULL_USE function
#define NULL_USE( variable )                       \
    do {                                           \
        if ( 0 ) {                                 \
            auto static temp = (char *) &variable; \
            temp++;                                \
        }                                          \
    } while ( 0 )


// Include MPI
// clang-format off
#ifdef USE_MPI
    #include "mpi.h"
    int getRank() {
        int rank = 0;
        MPI_Comm_rank( MPI_COMM_WORLD, &rank );
        return rank;
    }
    int getSize() {
        int size = 0;
        MPI_Comm_size( MPI_COMM_WORLD, &size );
        return size;
    }
    void barrier() { MPI_Barrier( MPI_COMM_WORLD ); }
    int sumReduce( int x ) {
        int y;
        MPI_Allreduce( &x, &y, 1, MPI_INT, MPI_SUM, MPI_COMM_WORLD );
        return y;
    }
    int startup( int argc, char *argv[] ) {
        int provided_thread_support;
        MPI_Init_thread( &argc, &argv, MPI_THREAD_MULTIPLE, &provided_thread_support );
        return getRank();
    }
    void shutdown( ) {
        MPI_Barrier( MPI_COMM_WORLD );
        MPI_Finalize( );
    }
#else
    #define MPI_COMM_WORLD 0
    void MPI_Finalize() {}
    void barrier() {}
    int sumReduce( int x ) { return x; }
    int startup( int, char*[] ) { return 0; }
    void shutdown( ) { }
    int getRank() { return 0; }
    int getSize() { return 1; }
#endif
// clang-format on


// Class to store pass/failures
class UnitTest
{
public:
    void passes( const std::string &msg ) { passes_.push_back( msg ); }
    void failure( const std::string &msg ) { failure_.push_back( msg ); }
    void expected( const std::string &msg ) { expected_.push_back( msg ); }

    void print() const
    {
        int rank = getRank();
        if ( rank == 0 ) {
            std::cout << "\nTests passed:" << std::endl;
            for ( const auto &msg : passes_ )
                std::cout << "   " << msg << std::endl;
            std::cout << std::endl << "Tests expected failed:" << std::endl;
            printAll( expected_ );
            std::cout << std::endl << "Tests failed:" << std::endl;
            printAll( failure_ );
        } else {
            printAll( expected_ );
            printAll( failure_ );
        }
    }
    static void printAll( std::vector<std::string> messages )
    {
        int rank = getRank();
        int size = getSize();
        for ( int i = 0; i < size; i++ ) {
            if ( rank == i )
                for ( const auto &msg : messages )
                    std::cout << "   Rank " << rank << ": " << msg << std::endl;
            barrier();
        }
    }

    int N_failed() const { return sumReduce( failure_.size() ); }

    void clear()
    {
        passes_   = std::vector<std::string>();
        failure_  = std::vector<std::string>();
        expected_ = std::vector<std::string>();
    }

private:
    std::vector<std::string> passes_;
    std::vector<std::string> failure_;
    std::vector<std::string> expected_;
};


// Subract two size_t numbers, returning the absolute value
size_t abs_diff( size_t a, size_t b ) { return ( a >= b ) ? a - b : b - a; }


// Zero and check memory block
void fill( void *x, uint8_t value, size_t bytes )
{
    memset( x, value, bytes );
    auto y    = reinterpret_cast<uint8_t *>( x );
    bool pass = true;
    for ( size_t i = 0; i < value; i++ )
        pass = pass && y[i] == value;
    if ( !pass )
        throw std::logic_error( "Failed to set memory" );
}


// Test source_location::current
void test_source_location( UnitTest &ut,
                           const source_location &source = source_location::current() )
{
    std::cout << "Testing StackTrace::source_location::current:\n";
    auto source2 = SOURCE_LOCATION_CURRENT();
    std::cout << "   file: " << source.file_name() << "(" << source.line() << ":" << source.column()
              << ")  " << source.function_name() << '\n';
    std::cout << "   file: " << source2.file_name() << "(" << source2.line() << ":"
              << source2.column() << ")  " << source2.function_name() << '\n';
    bool test1 = std::string( source.function_name() ).find( "main" ) != std::string::npos;
    bool test2 =
        std::string( source2.function_name() ).find( "test_source_location" ) != std::string::npos;
    if ( test1 && test2 )
        ut.passes( "source_location::current()" );
    else if ( test2 && source.empty() )
        ut.expected( "source_location::current()" );
    else
        ut.failure( "source_location::current()" );
}


/****************************************************************
 * Run some basic utility tests                                  *
 ****************************************************************/
int main( int argc, char *argv[] )
{
    int rank = startup( argc, argv );

    // Limit the scope of variables
    int num_failed = 0;
    {
        UnitTest ut;

        // Check the OS
        constexpr auto OS = Utilities::getOS();
        if ( OS == Utilities::OS::Linux )
            ut.passes( "OS: Linux" );
        else if ( OS == Utilities::OS::Windows )
            ut.passes( "OS: Windows" );
        else if ( OS == Utilities::OS::macOS )
            ut.passes( "OS: macOS" );
        else
            ut.failure( "Known OS" );

        // Test source_location
        test_source_location( ut );

        // Test getSystemMemory
        size_t system_bytes = Utilities::getSystemMemory();
        std::cout << "Total system bytes = " << system_bytes << std::endl;
        if ( system_bytes > 0 )
            ut.passes( "getSystemMemory" );
        else
            ut.failure( "getSystemMemory" );

        // Test the memory usage
        double t0       = Utilities::time();
        size_t n_bytes1 = Utilities::getMemoryUsage();
        double time1    = Utilities::time() - t0;
        auto *tmp       = new uint64_t[0x100000];
        fill( tmp, 0xAA, 0x100000 * sizeof( uint64_t ) );
        t0              = Utilities::time();
        size_t n_bytes2 = Utilities::getMemoryUsage();
        double time2    = Utilities::time() - t0;
        delete[] tmp;
        t0              = Utilities::time();
        size_t n_bytes3 = Utilities::getMemoryUsage();
        double time3    = Utilities::time() - t0;
        if ( rank == 0 ) {
            std::cout << "Number of bytes used for a basic test: " << n_bytes1 << ", " << n_bytes2
                      << ", " << n_bytes3 << std::endl;
            std::cout << "   Time to query: " << time1 * 1e6 << " us, " << time2 * 1e6 << " us, "
                      << time3 * 1e6 << " us" << std::endl;
        }
        if ( n_bytes1 == 0 ) {
            ut.failure( "getMemoryUsage returns 0" );
        } else {
            ut.passes( "getMemoryUsage returns non-zero" );
            if ( n_bytes2 > n_bytes1 ) {
                ut.passes( "getMemoryUsage increases size" );
            } else if ( OS == Utilities::OS::macOS ) {
                ut.expected( "getMemoryUsage does not increase size" );
            } else {
                ut.failure( "getMemoryUsage increases size" );
            }
            if ( n_bytes1 == n_bytes3 ) {
                ut.passes( "getMemoryUsage decreases size properly" );
            } else if ( OS != Utilities::OS::Linux ) {
                ut.expected( "getMemoryUsage does not decrease size properly" );
            } else {
                ut.failure( "getMemoryUsage does not decrease size properly" );
            }
        }

        // Run large memory test of getMemoryUsage
        if ( system_bytes >= 4e9 && rank == 0 ) {
            // Test getting the memory usage for 2-4 GB bytes
            // Note: we only run this test on machines with more than 4 GB of memory
            n_bytes1   = Utilities::getMemoryUsage();
            auto *tmp2 = new uint64_t[0x10000001]; // Allocate 2^31+8 bytes
            fill( tmp2, 0xAA, 0x10000001 * sizeof( uint64_t ) );
            n_bytes2 = Utilities::getMemoryUsage();
            for ( int i = 0; i < 10; i++ ) {
                if ( ( tmp2[rand() % 0x1000000] & 0xFF ) != 0xAA )
                    ut.failure( "Internal error" );
            }
            delete[] tmp2;
            tmp2     = nullptr;
            n_bytes3 = Utilities::getMemoryUsage();
            if ( n_bytes2 > 0x80000000 && n_bytes2 < n_bytes1 + 0x81000000 &&
                 abs_diff( n_bytes1, n_bytes3 ) < 50e3 ) {
                ut.passes( "getMemoryUsage correctly handles 2^31 - 2^32 bytes" );
            } else {
                std::cout << "Memtest 2-4 GB failes: " << n_bytes1 << " " << n_bytes2 << " "
                          << n_bytes3 << std::endl;
                ut.failure( "getMemoryUsage correctly handles 2^31 - 2^32 bytes" );
            }
        }
        if ( system_bytes >= 8e9 && rank == 0 ) {
            // Test getting the memory usage for > 4 GB bytes
            // Note: we only run this test on machines with more than 8 GB of memory
            n_bytes1    = Utilities::getMemoryUsage();
            size_t size = 0x20000000;
            auto *tmp2  = new uint64_t[size]; // Allocate 2^31+8 bytes
            if ( tmp2 == nullptr ) {
                ut.expected( "Unable to allocate variable of size 4 GB" );
            } else {
                fill( tmp2, 0xAA, size * sizeof( uint64_t ) );
                n_bytes2 = Utilities::getMemoryUsage();
                for ( int i = 0; i < 10; i++ ) {
                    if ( ( tmp2[rand() % size] & 0xFF ) != 0xAA )
                        ut.failure( "Internal error" );
                }
                delete[] tmp2;
                tmp2 = nullptr;
                NULL_USE( tmp2 );
                n_bytes3 = Utilities::getMemoryUsage();
                if ( n_bytes2 > 0x100000000 && n_bytes2 < n_bytes1 + 0x110000000 &&
                     abs_diff( n_bytes1, n_bytes3 ) < 50e3 ) {
                    ut.passes( "getMemoryUsage correctly handles memory > 2^32 bytes" );
                } else {
                    std::cout << "Memtest >4 GB fails: " << n_bytes1 << " " << n_bytes2 << " "
                              << n_bytes3 << std::endl;
                    ut.expected( "getMemoryUsage does not handle memory > 2^32 bytes" );
                }
            }
        }

        // Test getting the executable
        std::string exe = StackTrace::getExecutable();
        if ( rank == 0 )
            std::cout << "Executable: " << exe << std::endl;
        if ( exe.find( "TestUtilities" ) != std::string::npos )
            ut.passes( "getExecutable" );
        else
            ut.failure( "getExecutable" );

        // Test catching an error
        try {
            Utilities::abort( "test_error", SOURCE_LOCATION_CURRENT() );
            ut.failure( "Failed to catch error" );
        } catch ( const StackTrace::abort_error &err ) {
            if ( err.message == "test_error" )
                ut.passes( "Caught error" );
            else
                ut.failure( "Failed to catch error with proper message" );
        } catch ( std::exception &err ) {
            ut.failure( "Caught unknown exception type" );
        }

        // Check if we are running through valgrind and print the result
        if ( Utilities::running_valgrind() )
            std::cout << "Running through valgrind\n";
        else
            std::cout << "Not running through valgrind\n";

        // Finished testing, report the results
        ut.print();
        num_failed = ut.N_failed();
    }

    // Shutdown
    shutdown();

    // Finished successfully
    return num_failed;
}
