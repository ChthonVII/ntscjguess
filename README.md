# ntscjguess

**Superceded by gamutthingy**

Finds the NTSC-J color that converts to a given sRGB color.

Principally intended for fixing Final Fantasy 7 ESUI themes that have their text box backgrounds broken by the change to treat hardcoded colors as using the NTSC-J color gamut ([72c329e](https://github.com/ChthonVII/FFNx/commit/72c329ebf727830633ddeac6abb5d41c1aff2c83)).
This tool allows you to find the NTSC-J color that will yield a desired sRGB color when FFNx is done converting it.

Usage:
`ntscjguess input_color`  
Input_color should be gamma-encoded RGB8 in the format `0xRRGGBB`.  
Output will be a console message printed to stdout.

To build on Linux:
`gcc -o ntscjguess ntscjguess.c -lm`

Useful notes about ESUI themes:
- The 1color.txt hext files used by Finishing Touch look like this:
```
..\ff7.exe

9261F8 = B0 58 00 80 50 3F 00 80 80 5C 00 80 20 00 00
91EFC8 = B0 58 00 80 50 3F 00 80 80 5C 00 80 20 00 00 80 00 58 B0 00 3F 50 00 5C 80 00 00 20
```
- The first line is four BGRA8 values (missing the alpha component for the last one).
- The second line is the same four BGRA8 values (this time including the alpha component for the last one) followed by four RGB8 values for the same colors.
- The remaining files just copy the second line with various Delay values.
- Just plug the colors into ntscjguess and revise the hext files accordingly. For instance, the above example revises to this:
```
..\ff7.exe

9261F8 = AB 4B 00 80 4E 3D 00 80 7D 57 00 80 1E 00 02
91EFC8 = AB 4B 00 80 4E 3D 00 80 7D 57 00 80 1E 00 02 80 00 4B AB 00 3D 4E 00 57 7D 02 00 1E
```
