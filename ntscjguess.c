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

// RGB <--> XYZ matrices, assuming sRGB gamut for RGB
const float RGBtoXYZMatrix[3][3] = {
    {0.412410846488539, 0.357584567852952, 0.180453803933608},
    {0.212649342720653, 0.715169135705904, 0.072181521573443},
    {0.01933175842915, 0.119194855950984, 0.950390034050337}
};
const float XYZtoRGBMatrix[3][3] = {
    {3.24081239889528, -1.53730844562981, -0.498586522906966},
    {-0.969243017008641, 1.87596630290857, 0.041555030856686},
    {0.055638398436113, -0.204007460932414, 1.05712957028614}
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



// sRGB gamma functions
float togamma(float input){
    if (input <= 0.0031308){
        return clampfloat(input * 12.92);
    }
    return clampfloat((1.055 * pow(input, (1.0/2.4))) - 0.055);
}
float tolinear(float input){
    if (input <= 0.04045){
        return clampfloat(input / 12.92);
    }
    return clampfloat(pow((input + 0.055) / 1.055, 2.4));
}

struct pixel {
    float red;
    float green;
    float blue;
};

struct pixel8{
    unsigned char red;
    unsigned char green;
    unsigned char blue;
};

void printpixel(struct pixel input){
    printf("red: %f, green: %f, blue %f\n", input.red, input.green, input.blue);
    return;
}

struct pixel pixelfromint(long int input){
    struct pixel output;
    output.red = rgbtofloat(input >> 16);
    output.green = rgbtofloat((input & 0x0000FF00) >> 8);
    output.blue = rgbtofloat(input & 0x000000FF);
    return output;
}

struct pixel pixelfrompixel8(struct pixel8 input){
    struct pixel output;
    output.red = rgbtofloat(input.red);
    output.green = rgbtofloat(input.green);
    output.blue = rgbtofloat(input.blue);
    return output;
}

struct pixel8 pixel8frompixel(struct pixel input){
    struct pixel8 output;
    output.red = rgbtoint(input.red);
    output.green = rgbtoint(input.green);
    output.blue = rgbtoint(input.blue);
    return output;
}

struct pixel pixeltolinear(struct pixel input){
    struct pixel output;
    output.red = tolinear(input.red);
    output.green = tolinear(input.green);
    output.blue = tolinear(input.blue);
    return output;
}

struct pixel pixeltogamma(struct pixel input){
    struct pixel output;
    output.red = togamma(input.red);
    output.green = togamma(input.green);
    output.blue = togamma(input.blue);
    return output;
}

struct pixel NTSCJtoSRGB(struct pixel input){
    struct pixel output;
    
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

struct pixel RGBtoXYZ(struct pixel input){
    struct pixel output;
    
    output.red = RGBtoXYZMatrix[0][0] * input.red + RGBtoXYZMatrix[0][1] * input.green + RGBtoXYZMatrix[0][2] * input.blue;
    output.green = RGBtoXYZMatrix[1][0] * input.red + RGBtoXYZMatrix[1][1] * input.green + RGBtoXYZMatrix[1][2] * input.blue;
    output.blue = RGBtoXYZMatrix[2][0] * input.red + RGBtoXYZMatrix[2][1] * input.green + RGBtoXYZMatrix[2][2] * input.blue;
    
    // clamp values to 0 to 1 range
    output.red = clampfloat(output.red);
    output.green = clampfloat(output.green);
    output.blue = clampfloat(output.blue);
    
    return output;
}

struct pixel XYZtoRGB(struct pixel input){
    struct pixel output;
    
    output.red = XYZtoRGBMatrix[0][0] * input.red + XYZtoRGBMatrix[0][1] * input.green + XYZtoRGBMatrix[0][2] * input.blue;
    output.green = XYZtoRGBMatrix[1][0] * input.red + XYZtoRGBMatrix[1][1] * input.green + XYZtoRGBMatrix[1][2] * input.blue;
    output.blue = XYZtoRGBMatrix[2][0] * input.red + XYZtoRGBMatrix[2][1] * input.green + XYZtoRGBMatrix[2][2] * input.blue;
    
    // clamp values to 0 to 1 range
    output.red = clampfloat(output.red);
    output.green = clampfloat(output.green);
    output.blue = clampfloat(output.blue);
    
    return output;
}

// start with 8bit pixel, convert to float, linearize, gamut convert, convert to XYZ
struct pixel processpixel8(struct pixel8 input){
    return RGBtoXYZ(NTSCJtoSRGB(pixeltolinear(pixelfrompixel8(input))));
}

// distance between 2 pixels. should only be used in XYZ color space
float distance(struct pixel pixA, struct pixel pixB){
    float diffr = pixA.red - pixB.red;
    float diffg = pixA.green - pixB.green;
    float diffb = pixA.blue - pixB.blue;
    diffr = pow(diffr, 2.0);
    diffg = pow(diffg, 2.0);
    diffb = pow(diffb, 2.0);
    return pow(diffr + diffg + diffb, 0.5);
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
            struct pixel inputpixel = pixelfromint(input);
            struct pixel linearinputpixel = pixeltolinear(inputpixel);
            struct pixel goal = RGBtoXYZ(linearinputpixel);
            
            // start with the target as the first guess
            //struct pixel8 firstguess = pixel8frompixel(pixeltogamma(convertedinputpixel));
            struct pixel8 firstguess = pixel8frompixel(inputpixel);
            struct pixel firstguessoutput = processpixel8(firstguess);
            float firsterror = distance(firstguessoutput, goal);
            
            float besterror = firsterror;
            struct pixel8 bestguess = firstguess;
            
            while (true){
                bool foundbetterguess = false;
                struct pixel8 lastguess = bestguess;
                struct pixel8 newguess = bestguess;
                
                if (newguess.red < 255){
                    newguess.red++;
                    struct pixel newguessoutput = processpixel8(newguess);
                    float newerror = distance(newguessoutput, goal);
                    if (newerror < besterror){
                        foundbetterguess = true;
                        besterror = newerror;
                        bestguess = newguess;
                    }
                }
                
                newguess = lastguess;
                if (newguess.red > 0){
                    newguess.red--;
                    struct pixel newguessoutput = processpixel8(newguess);
                    float newerror = distance(newguessoutput, goal);
                    if (newerror < besterror){
                        foundbetterguess = true;
                        besterror = newerror;
                        bestguess = newguess;
                    }
                }
                
                newguess = lastguess;
                if (newguess.green < 255){
                    newguess.green++;
                    struct pixel newguessoutput = processpixel8(newguess);
                    float newerror = distance(newguessoutput, goal);
                    if (newerror < besterror){
                        foundbetterguess = true;
                        besterror = newerror;
                        bestguess = newguess;
                    }
                }
                
                newguess = lastguess;
                if (newguess.green > 0){
                    newguess.green--;
                    struct pixel newguessoutput = processpixel8(newguess);
                    float newerror = distance(newguessoutput, goal);
                    if (newerror < besterror){
                        foundbetterguess = true;
                        besterror = newerror;
                        bestguess = newguess;
                    }
                }
                
                newguess = lastguess;
                if (newguess.blue < 255){
                    newguess.blue++;
                    struct pixel newguessoutput = processpixel8(newguess);
                    float newerror = distance(newguessoutput, goal);
                    if (newerror < besterror){
                        foundbetterguess = true;
                        besterror = newerror;
                        bestguess = newguess;
                    }
                }
                
                newguess = lastguess;
                if (newguess.blue > 0){
                    newguess.blue--;
                    struct pixel newguessoutput = processpixel8(newguess);
                    float newerror = distance(newguessoutput, goal);
                    if (newerror < besterror){
                        foundbetterguess = true;
                        besterror = newerror;
                        bestguess = newguess;
                    }
                }
                
                // if the guess didn't improve, we are done
                if (!foundbetterguess){
                    printf("To achieve sRGB output of 0x%X use NTSC-J input of 0x%02X%02X%02X (red: %i, green: %i, blue: %i, error %f).\n", input, bestguess.red, bestguess.green, bestguess.blue, bestguess.red, bestguess.green, bestguess.blue, besterror);
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
