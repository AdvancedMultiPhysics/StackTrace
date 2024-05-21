from spack.package import *


class Stacktrace(CMakePackage):

    homepage = "https://bitbucket.org/mberrill/stacktrace/src/master/"
    git = "https://bitbucket.org/mberrill/stacktrace.git"

    version("0.0.90", tag="0.0.90")

    
    variant("mpi", default=True, description="build with mpi")

    depends_on("cmake@3.26.0:", type="build")
    depends_on("mpi", when="+mpi")

    def cmake_args(self):
        args = [
            "-DCMAKE_CXX_COMPILER=mpic++",
            "-DSTACKTRACE_INSTALL_DIR=" + self.prefix,
            self.define_from_variant("USE_MPI", "mpi"),

        ]
        return args

