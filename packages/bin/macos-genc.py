import sys
import glob
import os

EXIT_FAILURE = 1


class Params:
    def __init__(self, argv):
        self.output = "CMakeLists.txt"
        self.app_name = "MyGame"
        i = 0
        size = len(argv)
        while i < size:
            op = argv[i]
            i += 1
            if op == "-o":
                if i < size:
                    self.output = argv[i]
                    i += 1
                else:
                    print(f"missing value for op: {op}")
                    exit(EXIT_FAILURE)
            elif op == "--app":
                if i < size:
                    self.app_name = argv[i]
                    i += 1
                else:
                    print(f"missing value for op: {op}")
                    exit(EXIT_FAILURE)
            else:
                print(f"invalid op: {op}")
                exit(EXIT_FAILURE)


params = Params(sys.argv[1:])

APP_NAME = params.app_name
APP_ID = "com.cfrankb.creepspreadiii"
APP_VERSION = "1.0"
APP_LONGVERSION = "1.0.0"
APP_LONGNAME = "Creepspread III"
BUNDLE_NAME = APP_LONGNAME.replace(" ", "")


paths = ["src/*.cpp", "src/**/*.cpp", "src/**/**/*.cpp"]

src = []
for pattern in paths:
    for f in glob.glob(pattern):
        src.append(f)


CMAKEFILE_TEMPLATE = f"""
cmake_minimum_required(VERSION 3.16)
project({APP_NAME} LANGUAGES CXX)

set(CMAKE_VERBOSE_MAKEFILE ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64")


# Enable macOS bundle
set(MACOSX_BUNDLE_BUNDLE_NAME "{BUNDLE_NAME}")
set(MACOSX_BUNDLE_GUI_IDENTIFIER "{APP_ID}")
set(MACOSX_BUNDLE_SHORT_VERSION_STRING "{APP_VERSION}")
set(MACOSX_BUNDLE_LONG_VERSION_STRING "{APP_LONGVERSION}")
set(MACOSX_BUNDLE_ICON_FILE "{APP_NAME}.icns")
set(MACOSX_BUNDLE_BUNDLE_VERSION "100")  # Optional: internal build number
set(MACOSX_BUNDLE TRUE)
# execute_process(COMMAND git describe --tags OUTPUT_VARIABLE GIT_TAG OUTPUT_STRIP_TRAILING_WHITESPACE)
# set(MACOSX_BUNDLE_SHORT_VERSION_STRING "${{GIT_TAG}}")


#find_package(SDL3 REQUIRED)
#find_package(SDL3_mixer REQUIRED)
add_subdirectory(external/SDL3)
add_subdirectory(external/SDL3_mixer)
find_library(XMP_LIBRARY xmp)
find_library(ZLIB_LIBRARY z)
find_library(OGG_LIBRARY ogg)
find_library(VORBIS_LIBRARY vorbisfile)

add_executable({APP_NAME} MACOSX_BUNDLE
    {'\n\t'.join(src) + '\n'})

target_link_libraries({APP_NAME}
    PRIVATE SDL3::SDL3 SDL3_mixer::SDL3_mixer
    "-framework CoreFoundation"
    PUBLIC ${{XMP_LIBRARY}}
    ${{ZLIB_LIBRARY}}
    ${{OGG_LIBRARY}}
    ${{VORBIS_LIBRARY}}
)

"""

with open(params.output, "w") as tfile:
    tfile.write(CMAKEFILE_TEMPLATE.strip() + "\n")


"""
MyGame.app/
├── Contents/
│   ├── MacOS/
│   │   └── MyGame         ← your compiled binary
│   ├── Resources/
│   │   ├── assets/        ← images, sounds, etc.
│   │   └── config/        ← game settings, levels
│   └── Info.plist         ← metadata
"""
