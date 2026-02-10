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

#include "StackTrace/StackTraceMacros.h"
#include "StackTrace/Utilities.h"

#include <functional>


using namespace StackTrace;


// Class to store pass/failures
class UnitTest
{
public:
    void passes( const std::string &msg ) { passes_.push_back( msg ); }
    void failure( const std::string &msg ) { failure_.push_back( msg ); }
    void expected( const std::string &msg ) { expected_.push_back( msg ); }

    void print() const
    {
        printf( "\nTests passed:\n" );
        for ( const auto &msg : passes_ )
            printf( "   %s\n", msg.data() );
        printf( "\nTests expected failed:\n" );
        for ( const auto &msg : expected_ )
            printf( "   %s\n", msg.data() );
        printf( "\nTests failed:\n" );
        for ( const auto &msg : failure_ )
            printf( "   %s\n", msg.data() );
    }

    int N_failed() const { return failure_.size(); }

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


// Fill and check memory block (forces system to allocate buffer)
void fill( uint64_t *x, size_t N )
{
    uint64_t y = 0x9E3779B97F4A7C15;
    bool pass  = true;
    size_t Ns  = N < 10000 ? 1 : 13;
    for ( uint64_t i = 0; i < N; i += Ns ) {
        auto z = y ^ i;
        x[i]   = z;
        pass   = x[i] == z;
    }
    if ( !pass )
        throw std::logic_error( "Error writing data" );
}
void check( uint64_t *x, size_t )
{
    if ( x[0] != 0x9E3779B97F4A7C15 )
        throw std::logic_error( "Failed write" );
}


// Test source_location::current
void test_source_location( UnitTest &ut, const source_location &s1 = source_location::current() )
{
    printf( "Testing StackTrace::source_location::current:\n" );
    auto s2 = SOURCE_LOCATION_CURRENT();
    printf( "   %s (%u:%u)  %s\n", s1.file_name(), s1.line(), s1.column(), s1.function_name() );
    printf( "   %s (%u:%u)  %s\n", s2.file_name(), s2.line(), s2.column(), s2.function_name() );
    bool test1 = std::string( s1.function_name() ).find( "main" ) != std::string::npos;
    bool test2 =
        std::string( s2.function_name() ).find( "test_source_location" ) != std::string::npos;
    if ( test1 && test2 )
        ut.passes( "source_location::current()" );
    else if ( test2 && s1.empty() )
        ut.expected( "source_location::current()" );
    else
        ut.failure( "source_location::current()" );
}


/****************************************************************
 * Run some basic utility tests                                  *
 ****************************************************************/
int main( int, char *[] )
{
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
        using LLUINT        = long long unsigned int;
        LLUINT system_bytes = Utilities::getSystemMemory();
        printf( "Total system bytes = 0x%llx\n", system_bytes );
        if ( system_bytes > 0 )
            ut.passes( "getSystemMemory" );
        else
            ut.failure( "getSystemMemory" );

        // Test the memory usage
        double t0       = Utilities::time();
        LLUINT n_bytes1 = Utilities::getMemoryUsage();
        int time1       = 1e9 * ( Utilities::time() - t0 );
        auto *tmp       = new uint64_t[0x100000];
        fill( tmp, 0x100000 );
        t0              = Utilities::time();
        LLUINT n_bytes2 = Utilities::getMemoryUsage();
        int time2       = 1e9 * ( Utilities::time() - t0 );
        check( tmp, 0x100000 ); // Force a read to ensure memory is not freed early
        delete[] tmp;
        t0              = Utilities::time();
        LLUINT n_bytes3 = Utilities::getMemoryUsage();
        int time3       = 1e9 * ( Utilities::time() - t0 );
        printf( "Number of bytes used for a basic test: 0x%llx, 0x%llx, 0x%llx\n", n_bytes1,
                n_bytes2, n_bytes3 );
        printf( "   Time to query: %i ns, %i ns, %i ns\n", time1, time2, time3 );
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
        if ( system_bytes >= 4e9 ) {
            // Test getting the memory usage for 2-4 GB bytes
            // Note: we only run this test on machines with more than 4 GB of memory
            n_bytes1   = Utilities::getMemoryUsage();
            auto *tmp2 = new uint64_t[0x10000001]; // Allocate 2^31+8 bytes
            fill( tmp2, 0x10000001 );
            n_bytes2 = Utilities::getMemoryUsage();
            check( tmp2, 0x10000001 ); // Force a read to ensure memory is not freed early
            delete[] tmp2;
            tmp2     = nullptr;
            n_bytes3 = Utilities::getMemoryUsage();
            if ( n_bytes2 > 0x80000000 && n_bytes2 < n_bytes1 + 0x81000000 &&
                 abs_diff( n_bytes1, n_bytes3 ) < 0x1000000 ) {
                ut.passes( "Memtest 2-4 GB" );
            } else {
                char msg[4096];
                snprintf( msg, sizeof msg, "Memtest 2-4 GB: 0x%llx 0x%llx 0x%llx", n_bytes1,
                          n_bytes2, n_bytes3 );
                ut.failure( msg );
            }
        }
        if ( system_bytes >= 8e9 ) {
            // Test getting the memory usage for > 4 GB bytes
            // Note: we only run this test on machines with more than 8 GB of memory
            static_assert( sizeof( uint64_t ) == 8 );
            n_bytes1    = Utilities::getMemoryUsage();
            size_t size = 0x20000000;
            auto *tmp2  = new uint64_t[size]; // Allocate 2^31+8 bytes
            if ( tmp2 == nullptr ) {
                ut.expected( "Unable to allocate variable of size 4 GB" );
            } else {
                fill( tmp2, size );
                n_bytes2 = Utilities::getMemoryUsage();
                check( tmp2, size ); // Force a read to ensure memory is not freed early
                delete[] tmp2;
                tmp2     = nullptr;
                n_bytes3 = Utilities::getMemoryUsage();
                if ( n_bytes2 > 0x100000000 && n_bytes2 < n_bytes1 + 0x110000000 &&
                     abs_diff( n_bytes1, n_bytes3 ) < 0x1000000 ) {
                    ut.passes( "Memtest >4 GB" );
                } else {
                    char msg[4096];
                    snprintf( msg, sizeof msg, "Memtest >4 GB: 0x%llx 0x%llx 0x%llx", n_bytes1,
                              n_bytes2, n_bytes3 );
                    ut.failure( msg );
                }
            }
        }

        // Test getting the executable
        std::string exe = StackTrace::getExecutable();
        printf( "Executable: %s\n", exe.data() );
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
            printf( "Running through valgrind\n" );
        else
            printf( "Not running through valgrind\n" );

        // Finished testing, report the results
        ut.print();
        num_failed = ut.N_failed();
    }

    // Finished successfully
    return num_failed;
}
