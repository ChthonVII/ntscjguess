/*- ntscjguess
 *
 * COPYRIGHT: 2023 by Chris Bussard
 * LICENSE: GPLv3
 *
 * Finds the NTSC-J color that converts to a given sRGB color.
 * (Input and ouput are gamma-encoded with sRGB gamma function.)
 * 
 * To build on Linux:
 * gcc -o ntscjguess ntscjguess.c -lm
 * 
 */

#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include <math.h>
#include <errno.h>

// precomputed NTSC-J to SRGB color gamut conversion using Bradford Method
// Note:
// NTSC-J television sets had a whitepoint of 9300K+27mpcd (x=0.281, y=0.311)
// NTSC-J broadcasts had a whitepoint of 9300K+8mpcd (x=0.2838 y=0.2981)
// And neither of those is quite the same as CIE 9300K (x=0.2848 y=0.2932 or x=0.28315, y=0.29711, depending on which source you consult; discrepancy might relate to rivision of Planck's Law constants???)
// This matrix uses 9300K+27mpcd for NTSC-J white point and x=0.312713, y=0.329016 for D65 white point

const float ConversionMatrix[3][3] = {
    {1.34756301456925, -0.276463760747096, -0.071099263267176},
    {-0.031150036968175, 0.956512223260545, 0.074637860817515},
    {-0.024443490594835, -0.048150182045316, 1.07259361295816}
};

// RGB --> XYZ matrix, assuming sRGB gamut for RGB
const float RGBtoXYZMatrix[3][3] = {
    {0.412410846488539, 0.357584567852952, 0.180453803933608},
    {0.212649342720653, 0.715169135705904, 0.072181521573443},
    {0.01933175842915, 0.119194855950984, 0.950390034050337}
};

// clamp a float between 0.0 and 1.0
float clampfloat(float input){
    if (input < 0.0) return 0.0;
    if (input > 1.0) return 1.0;
    return input;
}

// RGB8 to float
float rgbtofloat(long int input){
    return (input / 255.0);
}

// float to RGB8
long int rgbtoint(float input){
    return (int)(input * 255.0);
}

// sRGB inverse gamma function
float tolinear(float input){
    if (input <= 0.04045){
        return clampfloat(input / 12.92);
    }
    return clampfloat(pow((input + 0.055) / 1.055, 2.4));
}

struct pixelf32 {
    float red;
    float green;
    float blue;
};

struct pixel8{
    unsigned char red;
    unsigned char green;
    unsigned char blue;
};

struct pixelf32 pixelfromint(long int input){
    struct pixelf32 output;
    output.red = rgbtofloat(input >> 16);
    output.green = rgbtofloat((input & 0x0000FF00) >> 8);
    output.blue = rgbtofloat(input & 0x000000FF);
    return output;
}

struct pixelf32 pixelfrompixel8(struct pixel8 input){
    struct pixelf32 output;
    output.red = rgbtofloat(input.red);
    output.green = rgbtofloat(input.green);
    output.blue = rgbtofloat(input.blue);
    return output;
}

struct pixel8 pixel8frompixel(struct pixelf32 input){
    struct pixel8 output;
    output.red = rgbtoint(input.red);
    output.green = rgbtoint(input.green);
    output.blue = rgbtoint(input.blue);
    return output;
}

struct pixelf32 pixeltolinear(struct pixelf32 input){
    struct pixelf32 output;
    output.red = tolinear(input.red);
    output.green = tolinear(input.green);
    output.blue = tolinear(input.blue);
    return output;
}

struct pixelf32 NTSCJtoSRGB(struct pixelf32 input){
    struct pixelf32 output;
    
    // Multiply by our pre-computed NTSC-J to sRGB Bradford matrix
    output.red = ConversionMatrix[0][0] * input.red + ConversionMatrix[0][1] * input.green + ConversionMatrix[0][2] * input.blue;
    output.green = ConversionMatrix[1][0] * input.red + ConversionMatrix[1][1] * input.green + ConversionMatrix[1][2] * input.blue;
    output.blue = ConversionMatrix[2][0] * input.red + ConversionMatrix[2][1] * input.green + ConversionMatrix[2][2] * input.blue;
    
    // clamp values to 0 to 1 range
    output.red = clampfloat(output.red);
    output.green = clampfloat(output.green);
    output.blue = clampfloat(output.blue);
    
    return output;
}

struct pixelf32 RGBtoXYZ(struct pixelf32 input){
    struct pixelf32 output;
    
    output.red = RGBtoXYZMatrix[0][0] * input.red + RGBtoXYZMatrix[0][1] * input.green + RGBtoXYZMatrix[0][2] * input.blue;
    output.green = RGBtoXYZMatrix[1][0] * input.red + RGBtoXYZMatrix[1][1] * input.green + RGBtoXYZMatrix[1][2] * input.blue;
    output.blue = RGBtoXYZMatrix[2][0] * input.red + RGBtoXYZMatrix[2][1] * input.green + RGBtoXYZMatrix[2][2] * input.blue;
    
    // clamp values to 0 to 1 range
    output.red = clampfloat(output.red);
    output.green = clampfloat(output.green);
    output.blue = clampfloat(output.blue);
    
    return output;
}

// start with 8bit pixel, convert to float, linearize, gamut convert, convert to XYZ
struct pixelf32 processpixel8(struct pixel8 input){
    return RGBtoXYZ(NTSCJtoSRGB(pixeltolinear(pixelfrompixel8(input))));
}

// distance between 2 pixels. should only be used in XYZ color space
float distance(struct pixelf32 pixA, struct pixelf32 pixB){
    float diffr = pixA.red - pixB.red;
    float diffg = pixA.green - pixB.green;
    float diffb = pixA.blue - pixB.blue;
    diffr = pow(diffr, 2.0);
    diffg = pow(diffg, 2.0);
    diffb = pow(diffb, 2.0);
    return pow(diffr + diffg + diffb, 0.5);
}

/*
 * Checks if input color plus offsets, when converted from NTSC-J to sRGB is closer to goal than current besterror
 * Returns true if so; else false
 * If true, sets besterror and bestguess accordingly, and sets saved offsets to inverse of offsets.
*/
bool checknearbycolor(struct pixel8 input, int roffset, int goffset, int boffset, struct pixelf32 goal, float* besterror, struct pixel8* bestguess, int* saveroffset, int* savegoffset, int* saveboffset){
    int newred = (int)input.red + roffset;
    int newgreen = (int)input.green + goffset;
    int newblue = (int)input.blue + boffset;
    if (newred < 0) return false;
    if (newred > 255) return false;
    if (newgreen < 0) return false;
    if (newgreen > 255) return false;
    if (newblue < 0) return false;
    if (newblue > 255) return false;
    struct pixel8 newcolor = {newred, newgreen, newblue};
    struct pixelf32 testoutput = processpixel8(newcolor);
    float newerror = distance(testoutput, goal);
    if (newerror <= *besterror){
        *besterror = newerror;
        *bestguess = newcolor;
        *saveroffset = roffset * -1;
        *savegoffset = goffset * -1;
        *saveboffset = boffset * -1;
        return true;
    }
    return false;
}

int main(int argc, const char **argv){
    
    int output = 1; // wrong arg count
    
    if (argc == 2){
        output = 2; // bad parameter
        char* endptr;
        errno = 0; //make sure errno is 0 before strtol()
        long int input = strtol(argv[1], &endptr, 0);
        bool inputok = true;
        // did we consume exactly 8 characters?
        if (endptr - argv[1] != 8){
            inputok = false;
        }
        // are there any chacters left in the input string?
        else if (*endptr != '\0'){
            inputok = false;
        }
        // is errno set?
        else if (errno != 0){
            inputok = false;
        }
        if (inputok){
            struct pixelf32 inputpixel = pixelfromint(input);
            struct pixelf32 linearinputpixel = pixeltolinear(inputpixel);
            struct pixelf32 goal = RGBtoXYZ(linearinputpixel);
            
            // start with the target as the first guess (this should be in the right neighborhood)
            struct pixel8 firstguess = pixel8frompixel(inputpixel);
            struct pixelf32 firstguessoutput = processpixel8(firstguess);
            float firsterror = distance(firstguessoutput, goal);
            
            float besterror = firsterror;
            struct pixel8 bestguess = firstguess;
            int lasti = 0;
            int lastj = 0;
            int lastk = 0;
            
            // hill climb to the input that converts to sRGB color closest to goal
            while (true){
                
                bool foundbetterguess = false;
                struct pixel8 thisguess = bestguess;
                int thisi = 0;
                int thisj = 0;
                int thisk = 0;
                
                // test increments of +/-1 on all axes (14 directions)
                for (int i = -1; i<=1; i++){
                    for (int j = -1; j<=1; j++){
                        for (int k = -1; k<=1; k++){
                            // skip the input color
                            if ((i == 0) && (j == 0) && (k == 0)) continue;
                            // don't go back to where we just came from
                            if ((i == lasti) && (j == lastj) && (k == lastk)) continue;
                            if(checknearbycolor(thisguess, i, j, k, goal, &besterror, &bestguess, &thisi, &thisj, &thisk)){
                                foundbetterguess = true;
                            }
                        }
                    }
                }
                // save out the reverse of the direction we just moved
                lasti = thisi;
                lastj = thisj;
                lastk = thisk;
                
                if (!foundbetterguess){
                    printf("To achieve sRGB output of 0x%06X use NTSC-J input of 0x%02X%02X%02X (red: %i, green: %i, blue: %i, error %f).\n", input, bestguess.red, bestguess.green, bestguess.blue, bestguess.red, bestguess.green, bestguess.blue, besterror);
                    break;
                }
                
            } // end of while true
            
            output = 0; // all good
        } // end if inputok
    } // end if argc = =2
    
    if (output !=0){
        printf("Usage: ntscjguess 0xRRGGBB\nWhere \"0xRRGGBB\" is 0x-prefixed hexadecimal representation of a RGB8 pixel value.\nThe input should be the sRGB pixel you wish to get as output when an unknown pixel in the NTSC-J gamut is converted to the sRGB gamut (using the sRGB gamma function in both directions).\nntscjguess will tell you the unknown pixel value.");
    }
    
    return output;
}
