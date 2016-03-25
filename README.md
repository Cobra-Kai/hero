# Hero

## Build Depencies

You will need automake/autoconf, SDL2 library and GL/GLU libraries.

### Windows + msys2

This assumes you have the msys2 package tool known as 'pacman'. Skip any packages that are already installed, it is not necessary to re-install them.

For 64-bit:
	pacman -S autoconf automake autogen base-devel
	pacman -S mingw-w64-x86_64-SDL2_ttf mingw-w64-x86_64-SDL2_net mingw-w64-x86_64-SDL2_image mingw-w64-x86_64-SDL2_gfx mingw-w64-x86_64-SDL2

For 32-bit:
	TBD

## Building

If you do not have a configure script, create one:

	autoreconf -i -I /usr/local/share/aclocal

Then configure and build the system:

	./configure
	make

### Windows + msys2

Because there is no pkg-config for "gl" in mingw or msys2, you will have to
manually specify the flags. This corrects the build to use opengl32.dll and
glu32.dll.

	GL_CFLAGS=" " GL_LIBS="-lopengl32" GLU_CFLAGS=" " GLU_LIBS="-lglu32" ./configure


## Running

Keyboard settings:

~~~
ESC - exit
W - forward
A - turn left
S - backward
D - turn right
Q - strafe left
E - strafe right
` - toggle text input
~~~

