tiny single-file game dev library. I made this mostly for myself, to develop my future games and to
have fun writing it all from scratch. feel free to use it though, it's public domain!

this is a very early work-in-progress. at its current state it's barely more than a platform layer.

all of the code and assets are public domain unless stated otherwise

# your first window

I want it to be as painless as possible to get something on screen so you can have fun prototyping
right away, while still giving a powerful interface to your gpu. drawing a rectangle is as simple as

```c
#define WEEBCORE_IMPLEMENTATION
#include "WeebCore.c"
#include "Platform/Platform.h"

Mesh mesh;

void Init() {
  mesh = MkMesh();
  Col(mesh, 0xff3333);
  Quad(mesh, 10, 10, 200, 100);
}

void Frame() {
  PutMesh(mesh, 0, 0);
}

void AppInit() {
  On(INIT, Init);
  On(FRAME, Frame);
}
```

note that the platform include is absolutely optional and you can implement your own platform later.
WeebCore.c does not use any external library and doesn't include anything, not even the standard C
library

# calling WeebCore from other languages (FFI)

WeebCore is designed to be FFI friendly. most of the interfaces take basic C types. the only gotcha
is that if your lang/FFI lib can't generate callbacks you have to handle msgs manually (through
a simple if/else or switch statement, or by building your own msg handling around it)

here's a minimal example in python with callbacks

```py
#!/usr/bin/env python

# run from WeebCore root dir with
# ./build.sh lib && vblank_mode=0 LD_LIBRARY_PATH="$(pwd)" /path/to/script.py

from ctypes import CDLL, c_void_p, c_int, c_float, CFUNCTYPE

weeb = CDLL('WeebCore.so')  # on windows you would change this to dll
weeb.MkMesh.restype = weeb.AppWnd.restype = weeb.MkMesh.restype = c_void_p
weeb.Col.argtypes = [c_void_p, c_int]
weeb.Quad.argtypes = [c_void_p] + [c_float] * 4
weeb.PutMesh.argtypes = [c_void_p] * 3

INIT = 6
FRAME = 7


class Game:
  def __init__(self):
    self.mesh = None


G = Game()


@CFUNCTYPE(None)
def init():
  G.mesh = weeb.MkMesh()
  weeb.Col(G.mesh, 0xff3333)
  weeb.Quad(G.mesh, 10, 10, 200, 100)


@CFUNCTYPE(None)
def frame():
  weeb.PutMesh(G.mesh, None, None)


weeb.On(INIT, init)
weeb.On(FRAME, frame)
weeb.AppMain(0, None)
```

here's a minimal example in python without callbacks

```py
#!/usr/bin/env python

# run from WeebCore root dir with
# ./build.sh lib && vblank_mode=0 LD_LIBRARY_PATH="$(pwd)" /path/to/script.py

from ctypes import CDLL, c_void_p, c_int, c_float

weeb = CDLL('WeebCore.so')  # on windows you would change this to dll
weeb.MkMesh.restype = weeb.AppWnd.restype = weeb.MkMesh.restype = c_void_p
weeb.Col.argtypes = [c_void_p, c_int]
weeb.Quad.argtypes = [c_void_p] + [c_float] * 4
weeb.PutMesh.argtypes = [c_void_p] * 3
weeb.SwpBufs.argtypes = weeb.NextMsg.argtypes = [c_void_p]

weeb.MkApp(0, None)
wnd = weeb.AppWnd()
mesh = weeb.MkMesh()
weeb.Col(mesh, 0xff3333)
weeb.Quad(mesh, 10, 10, 200, 100)

while weeb.AppRunning():
  while weeb.NextMsg(wnd) and weeb.AppHandleMsg():
    # TODO: handle msgs here
    pass

  weeb.AppFrame()
  weeb.PutMesh(mesh, None, None)
  weeb.SwpBufs(wnd)

weeb.RmApp()
```

# philosophy

* modular design - no platform specific code in the core code, just provide a platform interface
  that could be implemented for any platform
* failure is an option - cutting corners to make code simpler and programming more fun is fine.
  not trying to cover every use/edge case
* stick to C89 standard for maximum compatibility (platform specific code may use newer features)
* minimal boilerplate. you should be able to get something on screen with as few lines of code as
  possible without writing any long winded setup code
* FFI friendly: only expose functions and enums whenever possible, do not include any preprocessor
  conditional code in the interface.  C macros should all be optional or minor features
* minimal dependencies. instead of including a library, implement the minimal subset needed to do
  what you need to do
* minimal lines of code, but prioritize simplicity over code golfing. ideally the codebase stays
  small enough that the entire thing should compile almost instantly
* I want it to have its own tools and ecosystem to give it a bit more personality. for example I'm
  currently bootstrapping fonts and ui with a basic sprite editor I wrote and saving sprites to a
  custom wbspr format I've made. it should bootstrap itself, because it's fun.
* use short but easy to type names. disregard any potential for name clashes with other libraries
  as the focus is on minimal dependencies
* don't be afraid to break api if that means nicer/less code

# portability
at the moment the platform assumptions in the core logic are

* signed integers use two's complement and wrap around on overflow
* `int` is 32-bit
* `char` and `unsigned char` are 1 byte

the included platform layer implementations currently only support linux and similar unix-like OSes
running X11. feel free to port it to your favorite platform

see cflags.sh for recommended compiler flags

# why camelCase ? why this weird naming convention? it's ugly.
dunno, I guess I just got tired of typing underscores. I just felt like using this style for once.
it has the advantage of being much smoother to type even with longer names

# why are you using OpenGL 1 in your renderer implementation?
I like the simplicity of the api and not having to mess with extensions, plus it runs on the most
crippled platforms as a bonus. the renderer is modular anyway, I can always implement a OpenGL3+ or
vulkan backend if performance gets too bad

# yikes! global variables in the examples?
this project is all about breaking "rules" for simplicity. as your project grows in complexity you
will encapsulate stuff in structs as needed
