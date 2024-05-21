from spack.package import *


class Stacktrace(CMakePackage):

    homepage = "https://re-git.lanl.gov/xcap/oss/solvers/stacktrace"
    git = "ssh://git@re-git.lanl.gov:10022/xcap/oss/solvers/stacktrace.git"

    version("0.0.90", tag="0.0.90")

    
    variant("mpi", default=True, description="build with mpi")

    depends_on("cmake@3.29.2", type="build")
    depends_on("mpi", when="+mpi")

    def cmake_args(self):
        args = [
            "-DCMAKE_CXX_COMPILER=mpic++",
            "-DSTACKTRACE_INSTALL_DIR=" + self.prefix,
            self.define_from_variant("USE_MPI", "mpi"),

        ]
        return args

