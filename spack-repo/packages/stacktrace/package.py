from spack.package import *


class Stacktrace(CMakePackage):

    homepage = "https://bitbucket.org/mberrill/stacktrace/src/master/"
    git = "https://bitbucket.org/mberrill/stacktrace.git"

    version("0.0.90", tag="0.0.90")

    
    variant("mpi", default=True, description="build with mpi")
    variant("timer", default=False, description="include timerutility as a dependency")


    depends_on("cmake@3.26.0:", type="build")
    depends_on("mpi", when="+mpi")
    depends_on("timerutility+mpi", when="+timer+mpi")
    depends_on("timerutility~mpi", when="+timer~mpi")

    def cmake_args(self):
        args = [
            "-DSTACKTRACE_INSTALL_DIR=" + self.prefix,
            self.define_from_variant("USE_MPI", "mpi"),
        ]

        if self.spec.satisfies("^mpi"):
            args.append("-DCMAKE_CXX_COMPILER=mpicxx")
        return args

