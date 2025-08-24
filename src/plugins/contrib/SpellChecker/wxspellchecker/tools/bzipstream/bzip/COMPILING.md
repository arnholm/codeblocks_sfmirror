# Compiling bzip2

The following build systems are available for Bzip2:

* [Meson]: This is our preferred build system for Unix-like systems.
* [CMake]: Build tool for Unix and Windows.

Meson works for Unix-like OSes and Windows; nmake is only for Windows.

[Meson]: https://mesonbuild.com
[CMake]: https://cmake.org

> _Important note when compiling for Linux_:
>
> The SONAME for libbz2 for version 1.0 was: `libbz2.so.1.0`
> Some distros patched it to libbz2.so.1 to be supported by libtool.
> Others did not.
>
> We had to make a choice when switching from Makefiles -> CMake + Meson.
> So, the SONAME for libbz2 for version 1.1 is now: `libbz2.so.1`
>
> Distros that need it to be ABI compatible with the old SONAME may either:
> 1. Use CMake for the build with the option `-D USE_OLD_SONAME=ON`.
>    This will build an extra copy of the library with the old SONAME.
>
> 2. Use `patchelf --set-soname` after the build to change the SONAME and
>    install an extra symlink manually: `libbz2.so.1.0 -> libbz2.so.1.0.9`
>
> You can check the SONAME with: `objdump -p libbz2.so.1.0.9 | grep SONAME`

## Using Meson

Meson provides a [large number of built-in options](https://mesonbuild.com/Builtin-options.html)
to control compilation. A few important ones are listed below:

- -Ddefault_library=[static|shared|both], defaults to shared, if you wish to
  statically link libbz2 into the binaries set this to `static`
- --backend : defaults to ninja, use `vs` if you want to use msbuild
- --unity : This enables a unity build (sometimes called a jumbo build), makes a single build faster but rebuilds slower
- -Dbuildtype=[debug|debugoptmized|release|minsize|plain] : Controls default optimization/debug generation args,
  defaults to `debug`, use `plain` if you wish to pass your own cflags.

Meson recognizes environment variables like `$CFLAGS` and `$CC`.
It is recommended that you do not use `$CFLAGS`, and instead use `-Dc_args` and
`-DC_link_args`, as Meson will remember these even if you need to reconfigure
from scratch (such as when you update Meson), it will not remember `$CFLAGS`.

Meson will never change compilers once configured, so `$CC` is perfectly safe.

### Unix-like (Linux, *BSD, Cygwin, macOS)

You will need:
 - Python 3.6 or newer (for 3.5 for Meson and 3.6 for the tests)
 - Python's 'pytest' module, for running the tests.
 - meson (Version 0.56 or newer)
 - ninja
 - pkg-config
 - A C compiler such as GCC or Clang

Some linux distros package managers refer to ninja as ninja-build, fedora
and debian/ubuntu both do this. Your OS probably provides Meson, although
it may be too old, in that case you can use python3's pip to install Meson:

```sh
sudo pip3 install meson
```
or, for a user local install:
```sh
pip3 install --user meson
```

Once you have installed the dependencies, the following should work
to use the standard Meson configuration, a `builddir` for
compilation, and a `/usr` prefix for installation:

```sh
meson --prefix /usr builddir/
ninja -C builddir
meson test -C builddir --print-errorlogs
[sudo] ninja -C builddir install
```

You can use `meson configure builddir` to check configuration options.
Currently bzip only has one project specific option, which is to force the
generation of documentation on or off.

Ninja acepts many of the same arguments as make, although it will
automatically detect the number of CPU cores available and use an appropriate
number of threads.

### Windows

You will need:
- Python 3.6 or newer
- Meson
- Visual Studio 2015+ (Community edition is fine)

You can install Meson with Python's pip package manager:
```cmd
python -m pip install meson
```
or you can [download pre-bundled installers of meson directly from meson's github](https://github.com/mesonbuild/meson/releases).

Either should work fine for the purposes of building Bzip2.

You will also need pkg-config. There are many sources of pkg-config, I
recommend installing from Chocolatey because it's easy. Chocolatey can also
provide Ninja, though Ninja is not required on Windows if you want to use
msbuild.

If you want to use MSVC or a compatible compiler launch the associated
environment cmd to run Meson from; the environments required to make those
compilers work is quite complex otherwise.

Once you have all of that installed you can invoke Meson to configure the
build. By default Meson will generate a Ninja backend. If you would prefer to
use msbuild, pass the backend flag `--backend=vs`. MSVC (and compatible
compilers like clang-cl and ICL) work with Ninja as well.

```cmd
meson $builddir
ninja -C $builddir
meson test -C builddir --print-errorlogs
```

or:
```cmd
meson $builddir --backend=vs
cd $builddir
msbuild bzip2.sln /m
```

## Using CMake

### Requirements

For Linux/Unix:
 - Python 3.6 or newer (for 3.5 for Meson and 3.6 for the tests)
 - CMake (Version 3.12 or newer)
 - A C compiler such as GCC or Clang

For Windows:
- Python 3.6 or newer
- CMake
- Visual Studio 2015+ (Community edition is fine)

### Build instructions for Unix & Windows (CMake)

Bzip2 can be compiled with the [CMake] build system.
You can use these commands to build Bzip2 in a certain `build` directory.

#### Basic Release build

Linux/Unix:
```sh
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE="Release"
cmake --build .
ctest -V
```

Windows:
```ps1
mkdir build && cd build
cmake ..
cmake --build . --config Release
ctest -C Release -V
```

#### Basic Debug build

Linux/Unix:
```sh
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE="Debug"
cmake --build .
ctest -V
```

Windows:
```ps1
mkdir build && cd build
cmake ..
cmake --build . --config Debug
ctest -C Release -V
```

#### Build and install to a specific install location (prefix)

Linux/Unix:
```sh
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE="Release" -DCMAKE_INSTALL_PREFIX=install
cmake --build .
ctest -V
cmake --build . --target install
```

Windows:
```ps1
mkdir build && cd build
cmake .. -DCMAKE_INSTALL_PREFIX=install
cmake --build . --config Release
ctest -C Release -V
cmake --build . --config Release --target install
```

#### Other CMake Options

`ENABLE_EXAMPLES`: Default: `OFF`
Enabling this option will also build the "dlltest" example executable. E.g.:
```sh
mkdir build && cd build
cmake .. -DENABLE_EXAMPLES=ON
cmake --build .
```

`ENABLE_DOCS`: Default: `OFF`
Enabling this option will generate extra documentation. E.g.:
```sh
mkdir build && cd build
cmake .. -DENABLE_DOCS=ON
cmake --build .
```

`ENABLE_APP`: Default: `ON`
Disabling this option will prevent building `bzip` or any of the other programs
that come with `bzip2`. It will also disable the tests. E.g.:
```sh
mkdir build && cd build
cmake .. -DENABLE_APP=OFF
cmake --build .
```

`ENABLE_LIB_ONLY`: Default: `OFF`
Enabling this option is similar to disabling `ENABLE_APP`. Only libbz2 will be
compiled. It will also disable the tests. E.g.:
```sh
mkdir build && cd build
cmake .. -DENABLE_LIB_ONLY=ON
cmake --build .
```

`ENABLE_STATIC_LIB`: Default: `OFF`
Enabling this option will build a static version of libbz2.
If `ENABLE_SHARED_LIB` is also enabled, the apps will link with the shared one.
E.g.:
```sh
mkdir build && cd build
cmake .. -DENABLE_STATIC_LIB=ON
cmake --build .
```

`ENABLE_SHARED_LIB`: Default: `ON`
Disabling this option will not build a shared version of libbz2. You must enable
`ENABLE_STATIC_LIB` if you disable `ENABLE_SHARED_LIB`. E.g.:
```sh
mkdir build && cd build
cmake .. -DENABLE_SHARED_LIB=OFF -DENABLE_STATIC_LIB=ON
cmake --build .
```

`USE_OLD_SONAME`: Default: `OFF`
Enabling this option will build an extra copy of the shared library that uses
the old SONAME. This option is made available for some linux distributions that
still distribute libbz2 with the old SONAME. E.g.:
```sh
mkdir build && cd build
cmake .. -DUSE_OLD_SONAME=ON
cmake --build .
```

`ENABLE_STATIC_LIB_IS_PIC`: Default: `ON`
Disabling this option will make it so that building the static library will not
use the '-fPIC'/'-fPIE' compiler options. You may need to disable this option if
your compiler cannot generate position-independent code for your platform. E.g.:
```sh
mkdir build && cd build
cmake .. -DENABLE_STATIC_LIB_IS_PIC=OFF
cmake --build .
```
