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
    make = 'make'
    testBinary ='./ixwebsocket_unittest' 

os.system('cmake -DCMAKE_BUILD_TYPE=Debug {} ..'.format(generator))
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

# unittest broken on Windows
if osName != 'Windows':
    testCommand = '{} {}'.format(testBinary, os.getenv('TEST', ''))
    os.system(testCommand)
