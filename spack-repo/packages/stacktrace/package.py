# Copyright 2013-2024 Lawrence Livermore National Security, LLC and other
# Spack Project Developers. See the top-level COPYRIGHT file for details.
#
# SPDX-License-Identifier: (Apache-2.0 OR MIT)

from spack.package import *


class Stacktrace(CMakePackage):
    """A library to enable easy debugging of an application."""

    homepage = "https://github.com/AdvancedMultiPhysics/StackTrace"
    git = "https://github.com/AdvancedMultiPhysics/StackTrace.git"

    maintainers("bobby-philip", "gllongo", "rbberger")

    license("UNKNOWN")

    version("master", branch="master")
    version("0.0.94", tag="0.0.94", commit="2c3a64e0d3169295ffb2811703b5b246bbe8badc")
    version("0.0.93", tag="0.0.93", commit="cb068ee7733825036bbd4f9fda89b4f6e12d73b5")

    variant("mpi", default=True, description="build with mpi")
    variant("shared", default=False, description="Build shared libraries")
    variant("pic", default=False, description="Produce position-independent code")
    variant("timerutility", default=False, description="Build with support for TimerUtility")
    variant(
        "cxxstd",
        default="17",
        values=("17", "20", "23"),
        multi=False,
        description="C++ standard",
    )

    conflicts("cxxstd=20", when="@:0.0.94") #c++ 20 is only compatible with stacktrace master and up
    conflicts("cxxstd=23", when="@:0.0.94") #c++ 23 is only compatible with stacktrace master and up

    depends_on("c", type="build")
    depends_on("cxx", type="build")
    depends_on("fortran", type="build")

    depends_on("git", type="build")

    depends_on("cmake@3.26.0:", type="build")
    depends_on("mpi", when="+mpi")
    depends_on("timerutility", when="+timerutility")
    depends_on("timerutility+shared", when="+timerutility+shared")

    phases = ["cmake", "build"]

    def cmake_args(self):
        spec = self.spec
        options = [
            self.define("StackTrace_INSTALL_DIR", self.prefix),
            self.define_from_variant("USE_MPI", "mpi"),
            self.define("MPI_SKIP_SEARCH", False),
            self.define_from_variant("BUILD_SHARED_LIBS", "shared"),
            self.define_from_variant("CMAKE_POSITION_INDEPENDENT_CODE", "pic"),
            self.define_from_variant("CMAKE_CXX_STANDARD", "cxxstd"),
            self.define_from_variant("ENABLE_SHARED", "shared"),
            self.define("ENABLE_STATIC", not spec.variants["shared"].value),
            self.define("DISABLE_GOLD", True),
            self.define("CFLAGS", self.compiler.cc_pic_flag),
            self.define("CXXFLAGS", self.compiler.cxx_pic_flag),
            self.define("FFLAGS", self.compiler.fc_pic_flag),
            self.define('CMAKE_C_COMPILER',   spack_cc),
            self.define('CMAKE_CXX_COMPILER', spack_cxx),
            self.define('CMAKE_Fortran_COMPILER', spack_fc)
        ]
        return options

    @run_after("build")
    def filter_compilers(self):
        kwargs = {"ignore_absent": True, "backup": False, "string": True}
        filenames = [join_path(self.prefix, "TPLsConfig.cmake")]

        filter_file(spack_cc, self.compiler.cc, *filenames, **kwargs)
        filter_file(spack_cxx, self.compiler.cxx, *filenames, **kwargs)
        filter_file(spack_fc, self.compiler.fc, *filenames, **kwargs)
