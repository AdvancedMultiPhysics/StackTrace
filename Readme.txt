This is a stack trace program to enable easy debugging of an application.  It is intended to be used as a library for the application.


Building:
The build system is based on CMake.  The build directory has an example configure script.  A basic configure script is:
   cmake                              \
      -D CMAKE_CXX_COMPILER=mpic++    \
      -D CMAKE_BUILD_TYPE=Debug       \
      -D USE_MPI:BOOL=TRUE            \
      ../src

Where CMAKE_BUILD_TYPE is build type ("Debug" or "Release"), CMAKE_CXX_COMPILER is the C++ compilers (requires c++11), USE_MPI indicates if we want to enable MPI support.


Running the test:
There is a sample test in the test folder that will be build.  It demonstrates the different stack options available.  To run:
    ./TestStack
    mpirun -n 4 ./TestStack


Using in user program:
Build the stack trace library:
   make install
Include the appropriate headers (and set the include path):
   #include "StackTrace.h"
   #include "Utilities.h"
Link the library (e.g. libstacktrace.a)

