tiny single-file game dev library. I made this mostly for myself, to develop my future games and to
have fun writing it all from scratch. feel free to use it though, it's public domain!

this is a very early work-in-progress. at its current state it's barely more than a platform layer.
as higher level features are implemented, the boilerplate code in the examples will be minimized.

all of the code and assets are public domain unless stated otherwise

# philosophy

* modular design - no platform specific code in the core code, just provide a platform interface
  that could be implemented for any platform
* failure is an option - cutting corners to make code simpler and programming more fun is fine.
  not trying to cover every use/edge case
* stick to C89 standard for maximum compatibility (platform specific code may use newer features)
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
