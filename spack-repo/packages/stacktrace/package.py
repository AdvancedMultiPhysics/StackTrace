from spack.package import *


class Stacktrace(CMakePackage):

    homepage = "https://github.com/berrill/StackTrace"
    git = "https://github.com/berrill/StackTrace.git"

    version("master", branch="master")
    version("0.0.92", tag="0.0.92")
    version("0.0.90", tag="0.0.90")

    variant("mpi", default=True, description="build with mpi")
    variant("timer", default=False, description="include timerutility as a dependency")
    variant("shared", default=False, description="Build shared libraries")
    variant("pic", default=False, description="Produce position-independent code")

    depends_on("cmake@3.26.0:", type="build")
    depends_on("mpi", when="+mpi")
    depends_on("timerutility+mpi", when="+timer+mpi")
    depends_on("timerutility~mpi", when="+timer~mpi")

    def cmake_args(self):
        args = [
            self.define("STACKTRACE_INSTALL_DIR", self.prefix),
            self.define_from_variant("USE_MPI", "mpi"),
            self.define_from_variant("BUILD_SHARED_LIBS", "shared"),
            self.define_from_variant("CMAKE_POSITION_INDEPENDENT_CODE", "pic"),
        ]

        if self.spec.satisfies("^mpi"):
            args.append(self.define("CMAKE_CXX_COMPILER", self.spec['mpi'].mpicxx))
        return args
