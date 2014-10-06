Howdy,

This is the source code for Protozoa, as it currently sits on my hard disk.

I was planning to clean up the source before releasing it, but I really don't
have any free time to do that. I'd rather spend my free time coding new
things (sorry). The code is a mess (code written at 3 A.M. doesn't tend to
look very pretty). LOTS of things could have been done better, but hopefully
someone can learn a few things from this.

It's hereby placed in the public domain. But if you do anything with this code,
like packaging it for a Linux distribution or something, I'd LOVE to hear about
it.

It doesn't have a configure script or anything (sorry). To compile it, as far
as I can remember, you need these:

  * SDL
  * libopenal
  * libogg
  * libvorbis
  * libvorbisfile
  * libglew
  * flex/bison
  * nVidia Cg Toolkit

I think the Cg toolkit would be the only objection against packaging it for
Linux, but, come to think of it, it's not really necessary, there are very
few shaders that could be easily converted to GLSL. The only reason I didn't
use GLSL is that it's not supported by Intel's OpenGL drivers (even though
Intel's hardware is shader-capable... weird).

After compiling, the binary should be placed on a directory called /i686 (or
something else, depending on your architecture).

Thank you, and have fun!

  -- Mauro Persano
     17-Jun-2010
