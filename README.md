# Hello!

libetc2 decodes ETC2_RGB and ETC1_RGB texture formats into raw rgb24 data.
It is based off of the Khronos data specification.

# Why?

UnityPack and pillow did not appear to support ETC texture unpacking.

# Why not use ETCPACK?

I was only interested in decoding, and ETCPACK is designed to be built on
the Visual Studio platform. This is a decode-only reimplementation for
POSIX/GNU99.

# Usage

You can use it as such:

> ./etc2tool input.bin output.rgb

And it outputs raw rgb24 data to output.rgb, which can be converted into an
appropriate image format using various utilities such as pillow.
