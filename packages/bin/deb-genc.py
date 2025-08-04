#!/bin/python
import glob
from datetime import datetime
now = datetime.now().astimezone()

APP_LONG_NAME = 'Creepspread III'
APP_NAME = 'cs3-runtime'
APP_STATUS = 'stable'
URGENCY = 'medium'
AUTHOR = 'Francois Blanchette'
AUTHOR_EMAIL = 'fb2000x@yahoo.ca'
CONTACT = AUTHOR_EMAIL
DESTINATION = 'games'
DESCRIPTION = 'Vibrant retro action-puzzler with tight platforming and a pumping chiptune soundtrack'
LONG_DESCRIPTION = '''Fast-paced retro arcade game with tracker music.
 Creepspread III is an intense, retro-themed arcade experience that
 fuses classic gameplay with a pulse-pounding soundtrack. The game
 features a unique, dynamic audio system built on tracker music,
 alongside high-speed pixel graphics that deliver a challenging
 and nostalgic feel.'''

VERSION = '1.0.0'
try:
    with open('VERSION') as s:
        VERSION = s.read()
except:
    print(f"No VERSION file -- using {VERSION}")


CHANGES = '\n'.join([f"  * {line}" for line in [
    'Initial release',
    'Added tracker music support via libxmp',
    'Improved SDL2 rendering and input',
]])

CHANGELOG = f'''{APP_NAME} ({VERSION}) {APP_STATUS}; urgency={URGENCY}

{CHANGES}

 -- {AUTHOR} <{AUTHOR_EMAIL}>  {now.strftime("%a, %d %b %Y %H:%M:%S %z")}
'''

with open ('packages/data/debian/changelog', 'w') as t:
    t.write(CHANGELOG)

DEPENDS = ', '.join([
    "libc6 (>= 2.27)",
    'libsdl2',
    'libsdl2-mixer',
    'libxmp',
    'libz'
])

paths = ['src/*.cpp', "src/**/*.cpp", "src/**/**/*.cpp"]
src = []
for pattern in paths:
    for f in glob.glob(pattern):
        src.append(f)
lines = [
    'cmake_minimum_required(VERSION 3.10)',
    f'project({APP_NAME})',
    '',
    'set(CMAKE_CXX_STANDARD 17)',
    '',
    f'add_executable({APP_NAME} {" ".join(src)})',
    '',
    '# Find SDL2 and SDL2_mixer (system-installed)',
    #'find_package(SDL2 REQUIRED)',
    #'find_package(SDL2_mixer REQUIRED)',
    'find_package(ZLIB REQUIRED)',
    'find_package(PkgConfig REQUIRED)',
    '',
    '# SDL2',
    'pkg_check_modules(SDL2 REQUIRED sdl2)',
    'pkg_check_modules(SDL2_MIXER REQUIRED SDL2_mixer)',
    '',
    '# libxmp (or use custom find module if needed)',
    'find_path(XMP_INCLUDE_DIR xmp.h)',
    'find_library(XMP_LIBRARY NAMES xmp libxmp)',
    '',
    'if(NOT XMP_INCLUDE_DIR OR NOT XMP_LIBRARY)',
        'message(FATAL_ERROR "libxmp not found")',
    'endif()',
    '',
    '# Link libraries',
    f'target_link_libraries({APP_NAME}',
    '   ${ZLIB_LIBRARIES}',
    '   ${SDL2_LIBRARIES}',
    '   ${SDL2_MIXER_LIBRARIES}',
    '   ${XMP_LIBRARY}',
    ')'

    '',
    '# Install binary to standard games location',
    f'install(TARGETS {APP_NAME} DESTINATION {DESTINATION})',
    '',
    '# Install game data to /usr/share/mygame',
    f'install(DIRECTORY data/ DESTINATION share/{APP_NAME})',
    '',

    'if(CMAKE_STRIP)',
    f'  add_custom_command(TARGET {APP_NAME} POST_BUILD',
    '    COMMAND "${CMAKE_STRIP}" "$<TARGET_FILE:APP_NAME>"'.replace('APP_NAME', APP_NAME),
    '    COMMENT "Stripping binary to remove debug symbols")',
    'endif()',
    '',

    '# Game Info',
    f'set(APP_NAME "{APP_NAME}")',
    f'set(GAME_NAME "{APP_LONG_NAME}")',
    f'set(GAME_EXEC "{APP_NAME}")',
    f'set(GAME_ICON "{APP_NAME}.png")  # source icon',
    'set(GAME_ICON_PATH "${CMAKE_SOURCE_DIR}/packages/data/${GAME_ICON}")',
    '',

    '# Create desktop entry content',
    'set(DESKTOP_FILE_CONTENT',
    '"[Desktop Entry]\\n"',
    '"Name=${GAME_NAME}\\n"',
    '"Exec=${GAME_EXEC}\\n"',
    '"Icon=${GAME_EXEC}\\n"',
    '"Type=Application\\n"',
    '"Categories=Game;\\n"',
    ')',
    '',
    '# Create the .desktop file at build time',
    'set(DESKTOP_FILE_PATH "${CMAKE_BINARY_DIR}/${GAME_EXEC}.desktop")',
    'file(WRITE "${DESKTOP_FILE_PATH}" "${DESKTOP_FILE_CONTENT}")',
    '',
    '# Install .desktop file',
    'install(FILES "${DESKTOP_FILE_PATH}" DESTINATION share/applications)',
    '',
    '# Install icon',
    'install(FILES "${GAME_ICON_PATH}"',
    '    DESTINATION share/pixmaps)  # or use icons/hicolor/ for theme-aware install',
    '',
    '# Enable packaging',
    'set(CPACK_GENERATOR "DEB")',
    f'set(CPACK_PACKAGE_NAME "{APP_NAME}")',
    f'set(CPACK_PACKAGE_VERSION "{VERSION}")',
    f'set(CPACK_DEBIAN_PACKAGE_MAINTAINER "{AUTHOR} <{AUTHOR_EMAIL}>")',
    'set(CPACK_DEBIAN_PACKAGE_SECTION "games")',
    f'set(CPACK_DEBIAN_PACKAGE_DEPENDS "{DEPENDS}")',
    f'set(CPACK_PACKAGE_DESCRIPTION "{DESCRIPTION}")',
    f'set(CPACK_PACKAGE_CONTACT "{CONTACT}")',
    f'set(CPACK_DEBIAN_PACKAGE_DESCRIPTION "{LONG_DESCRIPTION}")',
    '',
    'file(COPY "${CMAKE_SOURCE_DIR}/packages/data/debian/changelog" DESTINATION ${CMAKE_BINARY_DIR})',
    'execute_process(COMMAND gzip -9 -c "${CMAKE_BINARY_DIR}/changelog"',
    '    OUTPUT_FILE "${CMAKE_BINARY_DIR}/changelog.gz")',
    'install(FILES "${CMAKE_BINARY_DIR}/changelog.gz"',
    '    DESTINATION share/doc/${CPACK_PACKAGE_NAME})',
    '',
    'install(FILES "${CMAKE_SOURCE_DIR}/packages/data/debian/copyright"',
    '    DESTINATION share/doc/${CPACK_PACKAGE_NAME})',
    '',
    'execute_process(',
    '    COMMAND gzip -9 -c "${CMAKE_SOURCE_DIR}/packages/data/debian/APP_NAME.6"'
        .replace('APP_NAME', APP_NAME),
    '    OUTPUT_FILE "${CMAKE_BINARY_DIR}/APP_NAME.6.gz"'
        .replace('APP_NAME', APP_NAME),
    ')',
    'install(FILES "${CMAKE_BINARY_DIR}/APP_NAME.6.gz"'
        .replace('APP_NAME', APP_NAME),
    '        DESTINATION share/man/man6)',
    '',
    'include(CPack)',
    '',
    'message(STATUS "Source dir is: ${CMAKE_SOURCE_DIR}")'
]

with open('CMakeLists.txt', 'w') as t:
    t.write('\n'.join(lines))

print('CMakeList.txt created')

