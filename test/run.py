import os
import platform
import shutil

osName = platform.system()
print('os name = {}'.format(osName))

root = os.path.dirname(os.path.realpath(__file__))
buildDir = os.path.join(root, 'build')

if not os.path.exists(buildDir):
    os.mkdir(buildDir)

os.chdir(buildDir)

if osName == 'Windows':
    generator = '-G"NMake Makefiles"'
    make = 'nmake'
    testBinary ='ixwebsocket_unittest.exe'
else:
    generator = ''
    make = 'make -j6'
    testBinary ='./ixwebsocket_unittest'

sanitizersFlags = {
    'asan': '-DSANITIZE_ADDRESS=On',
    'ubsan': '-DSANITIZE_UNDEFINED=On',
    'tsan': '-DSANITIZE_THREAD=On',
    'none': ''
}
sanitizer = 'tsan'
if osName == 'Linux':
    sanitizer = 'none'

sanitizerFlags = sanitizersFlags[sanitizer]

# if osName == 'Windows':
#     os.environ['CC'] = 'clang-cl'
#     os.environ['CXX'] = 'clang-cl'

cmakeCmd = 'cmake -DCMAKE_BUILD_TYPE=Debug {} {} ..'.format(generator, sanitizerFlags)
print(cmakeCmd)
ret = os.system(cmakeCmd)
assert ret == 0, 'CMake failed, exiting'

ret = os.system(make)
assert ret == 0, 'Make failed, exiting'

def findFiles(prefix):
    '''Find all files under a given directory'''

    paths = []

    for root, _, files in os.walk(prefix):
        for path in files:
            fullPath = os.path.join(root, path)

            if os.path.islink(fullPath):
                continue

            paths.append(fullPath)

    return paths

#for path in findFiles('.'):
#    print(path)

# We need to copy the zlib DLL in the current work directory
shutil.copy(os.path.join(
    '..',
    '..',
    'third_party',
    'ZLIB-Windows',
    'zlib-1.2.11_deploy_v140',
    'release_dynamic',
    'x64',
    'bin',
    'zlib.dll'), '.')

lldb = "lldb --batch -o 'run' -k 'thread backtrace all' -k 'quit 1'"
lldb = ""  # Disabled for now
testCommand = '{} {} {}'.format(lldb, testBinary, os.getenv('TEST', ''))
ret = os.system(testCommand)
assert ret == 0, 'Test command failed'
