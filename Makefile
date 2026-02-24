# Makefile (for formatting CMake files)

.PHONY: format

# Find all cmake files we want to format
CMAKE_FORMAT := $(shell find . -type f \( -name '*.cmake' -o -name 'CMakeLists.txt' \) )
REFORMAT = $(CMAKE_FORMAT:=.format)

ifndef VERBOSE
.SILENT:
endif

# Perl command string
PERL_CMD := s/"[^"]*"(*SKIP)(*F)|\(([^[:space:]])/(\ \1/g;  \
            s/"[^"]*"(*SKIP)(*F)|([^[:space:]])\)/\1 \)/g;  \
            s/"[^"]*"(*SKIP)(*F)|\)\)/\) \)/g;              \
            s/"[^"]*"(*SKIP)(*F)|\( \)/\(\)/g;              \
            s/"[^"]*"(*SKIP)(*F)|\$\(\ MAKE \)/\$\(MAKE\)/g; \
            s/"[^"]*"(*SKIP)(*F)|MACRO \(/MACRO\(/g;        \
            s/"[^"]*"(*SKIP)(*F)|FUNCTION \(/FUNCTION\(/g;  \
            s/"[^"]*"(*SKIP)(*F)|FOREACH \(/FOREACH\(/g;    \
            s/"[^"]*"(*SKIP)(*F)|ELSE \(/ELSE\(/g;          \
            s/"[^"]*"(*SKIP)(*F)|ENDIF \(/ENDIF\(/g;        \
            s/"[^"]*"(*SKIP)(*F)| \)\$$"/\)\$$"/g;          \
            s/"[^"]*"(*SKIP)(*F)| \)\$$"/\"\)\\" \)/g;      \
            s/"[^"]*"(*SKIP)(*F)|\( \\r\?\\n \)/\(\\r\?\\n\)/g; \
            s/\"\)/\" \)/g;

# Format target
cmake_format:
	@# Guard: ensure cmake-format exists before doing any work
	@command -v cmake-format >/dev/null 2>&1 || { \
	  echo "Error: 'cmake-format' not found in PATH."; \
	  exit 1; \
	}
%.format: % cmake_format
	cmake-format -i --enable-markup --literal-comment-pattern='^#' $<
	perl -i -pe 'next if /^\s*(?:"[^"]*"(*SKIP)(*F))*#/; $(PERL_CMD)' "$<"
	sed -E -i \
	    -e '/^[^#]/s/\^\( /\^\(/g' \
	    $<

format: $(REFORMAT)
	@echo "Formatting CMake files"; 


