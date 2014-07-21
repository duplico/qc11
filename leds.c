/*
 * leds.c
 *
 *  Created on: May 26, 2014
 *      Author: George
 */

#include "qcxi.h"
#include "leds.h"
#include "fonts.h"

// Auto-generated animation wave
//const spriteframe anim_wave[] = {{0b00000100, 0b00011010, 0b00000111, 0b00011010, 0b00000100, 0b00000000, 0b00000000, 0b00000000, 0},{0b00000100, 0b00011010, 0b00000111, 0b00011010, 0b00000010, 0b00000000, 0b00000000, 0b00000000, 0},{0b00000100, 0b00011010, 0b00000111, 0b00011010, 0b00000001, 0b00000000, 0b00000000, 0b00000000, 0},{0b00000100, 0b00011010, 0b00000111, 0b00011011, 0b00000000, 0b00000000, 0b00000000, 0b00000000, 0},{0b00000100, 0b00011010, 0b00000111, 0b00011010, 0b00000001, 0b00000000, 0b00000000, 0b00000000, 0},{0b00000100, 0b00011010, 0b00000111, 0b00011011, 0b00000000, 0b00000000, 0b00000000, 0b00000000, 0},{0b00000100, 0b00011010, 0b00000111, 0b00011010, 0b00000001, 0b00000000, 0b00000000, 0b00000000, 0},{0b00000100, 0b00011010, 0b00000111, 0b00011011, 0b00000000, 0b00000000, 0b00000000, 0b00000000, 0},{0b00000100, 0b00011010, 0b00000111, 0b00011010, 0b00000001, 0b00000000, 0b00000000, 0b00000000, 0},{0b00000100, 0b00011010, 0b00000111, 0b00011010, 0b00000010, 0b00000000, 0b00000000, 0b00000000, 0},{0b00000100, 0b00011010, 0b00000111, 0b00011010, 0b00000100, 0b00000000, 0b00000000, 0b00000000, 8},};
// Auto-generated animation walkin
const spriteframe anim_walkin[] = {{0b10000000, 0b00000000, 0b10000000, 0b00000000, 0b00000000, 0},{0b01000000, 0b10000000, 0b01000000, 0b10000000, 0b00000000, 0},{0b11000000, 0b11000000, 0b10000000, 0b10000000, 0b10000000, 0},{0b10010000, 0b10100000, 0b01000000, 0b01000000, 0b01000000, 0},{0b01010000, 0b01010000, 0b01110000, 0b00100000, 0b00100000, 0},{0b01010000, 0b01010000, 0b10101000, 0b01110000, 0b00100000, 8},};

// Auto-generated animation sprite_trick_armdance
spriteframe anim_sprite_trick_armdance[] = {{0b01010000, 0b01010000, 0b10101000, 0b01110000, 0b00100000, 0},{0b01010000, 0b01010000, 0b00100000, 0b11111000, 0b00100000, 0},{0b01010000, 0b01010000, 0b00100000, 0b11111000, 0b10101000, 0},{0b01010000, 0b01010000, 0b00100000, 0b11111000, 0b00101000, 0},{0b01010000, 0b01010000, 0b10100000, 0b11111000, 0b00101000, 0},{0b01010000, 0b01010000, 0b10100000, 0b11111000, 0b00100000, 0},{0b01010000, 0b01010000, 0b00101000, 0b11111000, 0b00100000, 0},{0b01010000, 0b01010000, 0b00100000, 0b11111000, 0b00100000, 0},{0b01010000, 0b01010000, 0b10100000, 0b11111000, 0b00101000, 0},{0b01010000, 0b01010000, 0b00100000, 0b11111000, 0b00100000, 0},{0b01010000, 0b01010000, 0b00100000, 0b11111000, 0b10100000, 0},{0b01010000, 0b01010000, 0b00100000, 0b11111000, 0b00101000, 0},{0b01010000, 0b01010000, 0b10100000, 0b11111000, 0b00100000, 0},{0b01010000, 0b01010000, 0b00100000, 0b11111000, 0b00101000, 0},{0b01010000, 0b01010000, 0b00100000, 0b11111000, 0b10101000, 0},{0b01010000, 0b01010000, 0b00100000, 0b11111000, 0b00100000, 0},{0b01010000, 0b01010000, 0b10101000, 0b11111000, 0b00100000, 0},{0b01010000, 0b01010000, 0b10101000, 0b11111000, 0b00100000, 0},{0b01010000, 0b01010000, 0b10101000, 0b01110000, 0b00100000, 8},};

// Auto-generated animation sprite_trick_ballet
spriteframe anim_sprite_trick_ballet[] = {{0b01010000, 0b01010000, 0b10101000, 0b01110000, 0b00100000, 0},{0b01010000, 0b01010000, 0b00100000, 0b11111000, 0b00100000, 0},{0b10010000, 0b01010000, 0b00100000, 0b01110000, 0b10101000, 0},{0b00010000, 0b01010000, 0b00110000, 0b00111100, 0b01010000, 0},{0b00010000, 0b00010000, 0b00110000, 0b00111000, 0b00010000, 0},{0b00010000, 0b00010000, 0b00010000, 0b00010000, 0b00010000, 0},{0b00010000, 0b00010000, 0b00011000, 0b00111000, 0b00010000, 0},{0b00010000, 0b00010100, 0b00011000, 0b01111100, 0b00010000, 0},{0b00010000, 0b00010000, 0b00011000, 0b00111000, 0b00010000, 0},{0b00010000, 0b00010000, 0b00010000, 0b00010000, 0b00010000, 0},{0b00010000, 0b00010000, 0b00110000, 0b00111000, 0b00010000, 0},{0b00010000, 0b01010000, 0b00110000, 0b01111100, 0b00010000, 0},{0b00010000, 0b00010000, 0b00110000, 0b00111000, 0b00010000, 0},{0b00010000, 0b00010000, 0b00010000, 0b00010000, 0b00010000, 0},{0b00010000, 0b00010000, 0b00011000, 0b00111000, 0b00010000, 0},{0b00010000, 0b00010100, 0b00011000, 0b01111100, 0b00010000, 0},{0b00010000, 0b00010000, 0b00011000, 0b00111000, 0b00010000, 0},{0b00010000, 0b00010000, 0b00010000, 0b00010000, 0b00010000, 0},{0b00010000, 0b00010000, 0b00110000, 0b00111000, 0b00010000, 0},{0b00010000, 0b01010000, 0b00110000, 0b01111100, 0b00010000, 0},{0b00010000, 0b01010000, 0b00110000, 0b01111100, 0b00010000, 0},{0b10010000, 0b01010000, 0b00100000, 0b11111000, 0b00100000, 0},{0b01010000, 0b01010000, 0b10101000, 0b01110000, 0b00100000, 8},};

// Auto-generated animation sprite_trick_companion
spriteframe anim_sprite_trick_companion[] = {{0b01010000, 0b01010000, 0b10101000, 0b01110000, 0b00100000, 0},{0b01010000, 0b01010000, 0b01100000, 0b01111000, 0b00100000, 0},{0b01000000, 0b01011000, 0b01100000, 0b01111000, 0b00100000, 0},{0b00000000, 0b01111000, 0b01100000, 0b01111000, 0b00000000, 0},{0b00000000, 0b01111000, 0b01101000, 0b00111000, 0b00000000, 0},{0b00000000, 0b00111000, 0b00101000, 0b00111000, 0b00000000, 0},{0b00000000, 0b00111000, 0b00101000, 0b00111000, 0b00000000, 0},{0b01111100, 0b01101100, 0b01101100, 0b01010100, 0b01111100, 0},{0b01111100, 0b01101100, 0b01101100, 0b01010100, 0b01111100, 0},{0b11101110, 0b11000110, 0b10000010, 0b10000010, 0b11010110, 0},{0b11101110, 0b11000110, 0b10000010, 0b10000010, 0b11010110, 0},{0b11000011, 0b10000001, 0b10000001, 0b10000001, 0b11000011, 0},{0b10000001, 0b10000001, 0b10000001, 0b10000001, 0b10000001, 0},{0b00000000, 0b00000000, 0b00000000, 0b00000000, 0b00000000, 0},{0b00000000, 0b00000000, 0b00000000, 0b00000000, 0b00000000, 0},{0b00000000, 0b00000000, 0b00000000, 0b00000000, 0b00000000, 0},{0b10000000, 0b00000000, 0b10000000, 0b00000000, 0b00000000, 0},{0b01000000, 0b10000000, 0b01000000, 0b10000000, 0b00000000, 0},{0b11000000, 0b11000000, 0b10000000, 0b10000000, 0b10000000, 0},{0b10010000, 0b10100000, 0b01000000, 0b01000000, 0b01000000, 0},{0b01010000, 0b01010000, 0b01110000, 0b00100000, 0b00100000, 0},{0b01010000, 0b01010000, 0b10101000, 0b01110000, 0b00100000, 8},};

// Auto-generated animation sprite_trick_handwave
spriteframe anim_sprite_trick_handwave[] = {{0b01010000, 0b01010000, 0b10101000, 0b01110000, 0b00100000, 0},{0b01010000, 0b01010000, 0b00101000, 0b11110000, 0b00100000, 0},{0b01010000, 0b01010000, 0b00100000, 0b01111000, 0b10100000, 0},{0b01010000, 0b01010000, 0b00100000, 0b01110000, 0b01101000, 0},{0b01010000, 0b01010000, 0b00100000, 0b01110000, 0b10110000, 0},{0b01010000, 0b01010000, 0b00100000, 0b11110000, 0b00101000, 0},{0b01010000, 0b01010000, 0b00100000, 0b01111000, 0b10100000, 0},{0b01010000, 0b01010000, 0b00100000, 0b01110000, 0b01101000, 0},{0b01010000, 0b01010000, 0b00100000, 0b01110000, 0b10110000, 0},{0b01010000, 0b01010000, 0b00100000, 0b11110000, 0b00101000, 0},{0b01010000, 0b01010000, 0b00100000, 0b01111000, 0b10100000, 0},{0b01010000, 0b01010000, 0b00100000, 0b01110000, 0b01101000, 0},{0b01010000, 0b01010000, 0b00100000, 0b01110000, 0b10110000, 0},{0b01010000, 0b01010000, 0b00100000, 0b01110000, 0b01101000, 0},{0b01010000, 0b01010000, 0b00100000, 0b01110000, 0b10110000, 0},{0b01010000, 0b01010000, 0b00100000, 0b01110000, 0b01101000, 0},{0b01010000, 0b01010000, 0b00100000, 0b01110000, 0b10101000, 0},{0b01010000, 0b01010000, 0b00100000, 0b11111000, 0b00100000, 0},{0b01010000, 0b01010000, 0b10101000, 0b01110000, 0b00100000, 8},};

// Auto-generated animation sprite_trick_headroll
spriteframe anim_sprite_trick_headroll[] = {{0b01010000, 0b01010000, 0b10101000, 0b01110000, 0b00100000, 0},{0b01010000, 0b01010000, 0b10101000, 0b01110000, 0b00010000, 0},{0b01010000, 0b01010000, 0b10101000, 0b01111000, 0b00000000, 0},{0b01010000, 0b01010000, 0b10101100, 0b01110000, 0b00000000, 0},{0b01010000, 0b01010100, 0b10101000, 0b01110000, 0b00000000, 0},{0b01010100, 0b01010000, 0b10101000, 0b01110000, 0b00000000, 0},{0b01010010, 0b01010000, 0b10101000, 0b01110000, 0b00000000, 0},{0b01010001, 0b01010000, 0b10101000, 0b01110000, 0b00000000, 0},{0b01010001, 0b01010000, 0b10101000, 0b01110000, 0b00000000, 0},{0b01010001, 0b01010000, 0b00100000, 0b11111000, 0b00000000, 0},{0b01010001, 0b01010000, 0b00100000, 0b01110000, 0b10001000, 0},{0b01010001, 0b01010000, 0b00100000, 0b01110000, 0b01010000, 0},{0b01010001, 0b01010000, 0b00100000, 0b01110000, 0b10001000, 0},{0b01010001, 0b01010000, 0b00100000, 0b01110000, 0b01010000, 0},{0b01010000, 0b01010000, 0b00100000, 0b01110000, 0b10001000, 0},{0b01010000, 0b01010000, 0b00100000, 0b01110000, 0b10101000, 0},{0b01010000, 0b01010000, 0b00100000, 0b01110000, 0b10101000, 0},{0b01010000, 0b01010000, 0b00100000, 0b11111000, 0b00100000, 0},{0b01010000, 0b01010000, 0b10101000, 0b01110000, 0b00100000, 8},};

// Auto-generated animation sprite_trick_headstand
spriteframe anim_sprite_trick_headstand[] = {{0b01010000, 0b01010000, 0b10101000, 0b01110000, 0b00100000, 0},{0b01010000, 0b01010000, 0b00100000, 0b11111000, 0b00100000, 0},{0b01010000, 0b01010000, 0b00100000, 0b11111000, 0b00000000, 0},{0b01010000, 0b01010000, 0b11111000, 0b00000000, 0b00000000, 0},{0b01010000, 0b11111000, 0b01110000, 0b00000000, 0b00000000, 0},{0b10101000, 0b01110000, 0b01110000, 0b01000000, 0b00000000, 0},{0b10101000, 0b01110000, 0b00100000, 0b01110000, 0b01000000, 0},{0b10101000, 0b01110000, 0b00100000, 0b01010000, 0b01010000, 0},{0b10101000, 0b01110000, 0b00100000, 0b01010000, 0b01010000, 0},{0b10101000, 0b01110000, 0b00100000, 0b01010000, 0b01010000, 0},{0b10101000, 0b01110000, 0b00100000, 0b01010000, 0b01010000, 0},{0b10101000, 0b01110000, 0b00100000, 0b01010000, 0b00000000, 0},{0b10101000, 0b01110000, 0b01110000, 0b00000000, 0b00000000, 0},{0b11111000, 0b01110000, 0b00100000, 0b00000000, 0b00000000, 0},{0b01010000, 0b11111000, 0b01110000, 0b00100000, 0b00000000, 0},{0b01010000, 0b01010000, 0b10101000, 0b01110000, 0b00100000, 8},};

// Auto-generated animation sprite_trick_jump
spriteframe anim_sprite_trick_jump[] = {{0b01010000, 0b01010000, 0b10101000, 0b01110000, 0b00100000, 0},{0b01010000, 0b01110000, 0b11111000, 0b00100000, 0b00000000, 0},{0b01010000, 0b01010000, 0b01110000, 0b11111000, 0b00100000, 0},{0b00000000, 0b10001000, 0b01110000, 0b11111000, 0b00100000, 0},{0b00000000, 0b10001000, 0b01110000, 0b01110000, 0b10101000, 0},{0b00000000, 0b10001000, 0b01110000, 0b01110000, 0b10101000, 0},{0b00000000, 0b10001000, 0b01110000, 0b01110000, 0b10101000, 0},{0b10001000, 0b01110000, 0b01110000, 0b10101000, 0b00000000, 0},{0b10001000, 0b01010000, 0b00100000, 0b01110000, 0b10101000, 0},{0b01001000, 0b01010000, 0b00100000, 0b11111000, 0b00100000, 0},{0b01010000, 0b01010000, 0b10101000, 0b01110000, 0b00100000, 8},};

// Auto-generated animation sprite_trick_legstand
spriteframe anim_sprite_trick_legstand[] = {{0b01010000, 0b01010000, 0b10101000, 0b01110000, 0b00100000, 0},{0b01000000, 0b01011000, 0b10101000, 0b01110000, 0b00100000, 0},{0b01000000, 0b01000000, 0b10111000, 0b01111000, 0b00100000, 0},{0b01000000, 0b01000000, 0b10110000, 0b01111000, 0b00101100, 0},{0b01000000, 0b01000000, 0b10110000, 0b01111000, 0b00101100, 0},{0b01000000, 0b01000000, 0b10110000, 0b01111000, 0b00101100, 0},{0b01000000, 0b01000000, 0b10110000, 0b01111000, 0b00101100, 0},{0b01000000, 0b00100000, 0b01011000, 0b00111100, 0b00010110, 0},{0b01000000, 0b01111000, 0b00111100, 0b00010110, 0b00000000, 0},{0b01111000, 0b00111111, 0b00001011, 0b00000000, 0b00000000, 0},{0b01111111, 0b00111111, 0b00001000, 0b00000000, 0b00000000, 0},{0b01111111, 0b00111111, 0b00001000, 0b00000000, 0b00000000, 0},{0b01111110, 0b00111110, 0b00001000, 0b00000000, 0b00000000, 0},{0b01111100, 0b00111100, 0b00001000, 0b00000000, 0b00000000, 0},{0b01000100, 0b00111100, 0b00111000, 0b00010000, 0b00000000, 0},{0b01000100, 0b00101000, 0b00111000, 0b00111000, 0b00010000, 0},{0b01001000, 0b00101000, 0b01010100, 0b00111000, 0b00010000, 0},{0b01001000, 0b01010000, 0b10101000, 0b01110000, 0b00100000, 0},{0b01010000, 0b01010000, 0b10101000, 0b01110000, 0b00100000, 8},};

// Auto-generated animation sprite_trick_poof
spriteframe anim_sprite_trick_poof[] = {{0b01010000, 0b01010000, 0b10101000, 0b01110000, 0b00100000, 0},{0b01010000, 0b01010000, 0b01110000, 0b01110000, 0b00100000, 0},{0b00000000, 0b01010000, 0b01110000, 0b01110000, 0b00100000, 0},{0b00000000, 0b01010000, 0b01110000, 0b00100000, 0b00000000, 0},{0b00000000, 0b00000000, 0b01110000, 0b00100000, 0b00000000, 0},{0b00000000, 0b00000000, 0b00100000, 0b00000000, 0b00000000, 0},{0b00000000, 0b00000000, 0b00000000, 0b00000000, 0b00000000, 0},{0b00100000, 0b00100000, 0b11011000, 0b00100000, 0b00100000, 0},{0b10101000, 0b00000000, 0b10001000, 0b00000000, 0b10101000, 0},{0b10001000, 0b00000000, 0b00000000, 0b00000000, 0b10001000, 0},{0b00000000, 0b00000000, 0b00000000, 0b00000000, 0b00000000, 0},{0b00000000, 0b00000000, 0b00000000, 0b00000000, 0b00000000, 0},{0b00000000, 0b00000000, 0b00000000, 0b00000000, 0b00000000, 0},{0b00000000, 0b00000000, 0b00000000, 0b00000000, 0b00000000, 0},{0b00000000, 0b00000000, 0b00000000, 0b00000000, 0b00000000, 0},{0b00100000, 0b00000000, 0b00000000, 0b00000000, 0b00000000, 0},{0b01110000, 0b00100000, 0b00000000, 0b00000000, 0b00000000, 0},{0b10101000, 0b01110000, 0b00100000, 0b00000000, 0b00000000, 0},{0b01010000, 0b10101000, 0b01110000, 0b00100000, 0b00000000, 0},{0b01010000, 0b01010000, 0b10101000, 0b01110000, 0b00100000, 8},};

// Auto-generated animation sprite_trick_rocket
spriteframe anim_sprite_trick_rocket[] = {{0b01010000, 0b01010000, 0b10101000, 0b01110000, 0b00100000, 0},{0b01010000, 0b00100000, 0b10101000, 0b01110000, 0b00100000, 0},{0b00100000, 0b00100000, 0b10101000, 0b01110000, 0b00100000, 0},{0b00100000, 0b00100000, 0b10101000, 0b01110000, 0b00100000, 0},{0b00100000, 0b00100000, 0b10101000, 0b01110000, 0b00100000, 0},{0b00100000, 0b00100000, 0b10101000, 0b01110000, 0b00100000, 0},{0b11111000, 0b00100000, 0b10101000, 0b01110000, 0b00100000, 0},{0b11111000, 0b01100000, 0b10101000, 0b01110000, 0b00100000, 0},{0b11110000, 0b01110000, 0b10101000, 0b01110000, 0b00100000, 0},{0b11111000, 0b10110000, 0b00100000, 0b10101000, 0b01110000, 0},{0b01111000, 0b11011000, 0b00100000, 0b00100000, 0b10101000, 0},{0b11111000, 0b10110000, 0b00000000, 0b00100000, 0b00100000, 0},{0b11111100, 0b01101000, 0b00100000, 0b00000000, 0b00100000, 0},{0b11111100, 0b10110000, 0b00100000, 0b00000000, 0b00000000, 0},{0b11111100, 0b01101000, 0b00000000, 0b00000000, 0b00000000, 0},{0b01111000, 0b01110000, 0b00000000, 0b00000000, 0b00000000, 0},{0b01011000, 0b00000000, 0b00000000, 0b00000000, 0b00000000, 0},{0b00000000, 0b00000000, 0b00000000, 0b00000000, 0b00000000, 0},{0b00000000, 0b00000000, 0b00000000, 0b00000000, 0b00000000, 0},{0b00000000, 0b00000000, 0b00000000, 0b00000000, 0b00000000, 0},{0b00000000, 0b00000000, 0b00000000, 0b00000000, 0b00100000, 0},{0b00000000, 0b00000000, 0b00000000, 0b00100000, 0b00000000, 0},{0b00000000, 0b00000000, 0b00100000, 0b10001000, 0b01110000, 0},{0b00000000, 0b00100000, 0b10001000, 0b01110000, 0b00000000, 0},{0b00100000, 0b10001000, 0b01110000, 0b00000000, 0b00000000, 0},{0b10101000, 0b01110000, 0b00000000, 0b00000000, 0b00000000, 0},{0b11111000, 0b00100000, 0b00000000, 0b00000000, 0b00000000, 0},{0b00100000, 0b00000000, 0b00000000, 0b00000000, 0b00000000, 0},{0b00100000, 0b00100000, 0b00000000, 0b00000000, 0b00000000, 0},{0b01010000, 0b00100000, 0b01110000, 0b00100000, 0b00000000, 0},{0b01010000, 0b01010000, 0b10101000, 0b01110000, 0b00100000, 8},};

// Auto-generated animation sprite_trick_russian_slow
spriteframe anim_sprite_trick_russian_slow[] = {{0b01010000, 0b01010000, 0b10101000, 0b01110000, 0b00100000, 0},{0b01010000, 0b01110000, 0b11111000, 0b00100000, 0b00000000, 0},{0b01010000, 0b01110000, 0b01110000, 0b00100000, 0b00000000, 0},{0b01010000, 0b01110000, 0b01110000, 0b00100000, 0b00000000, 0},{0b10001000, 0b01110000, 0b01110000, 0b10101000, 0b00000000, 0},{0b01010000, 0b01110000, 0b01110000, 0b00100000, 0b00000000, 0},{0b10001000, 0b01110000, 0b01110000, 0b10101000, 0b00000000, 0},{0b01010000, 0b01110000, 0b01110000, 0b00100000, 0b00000000, 0},{0b10001000, 0b01110000, 0b01110000, 0b10101000, 0b00000000, 0},{0b10001000, 0b01010000, 0b00100000, 0b01110000, 0b10101000, 0},{0b01001000, 0b01010000, 0b00100000, 0b11111000, 0b00100000, 0},{0b01010000, 0b01010000, 0b10101000, 0b01110000, 0b00100000, 8},};

// Auto-generated animation sprite_trick_shuffle
spriteframe anim_sprite_trick_shuffle[] = {{0b01010000, 0b01010000, 0b10101000, 0b01110000, 0b00100000, 0},{0b01001000, 0b01010000, 0b10101000, 0b01110000, 0b00100000, 0},{0b01001000, 0b00101000, 0b00010100, 0b00111000, 0b00010000, 0},{0b00101000, 0b00101000, 0b01010100, 0b00111000, 0b00010000, 0},{0b00100100, 0b00101000, 0b01010100, 0b00111000, 0b00010000, 0},{0b00100100, 0b00010100, 0b00101010, 0b00011100, 0b00001000, 0},{0b00010100, 0b00010100, 0b00101010, 0b00011100, 0b00001000, 0},{0b00010010, 0b00010100, 0b00101010, 0b00011100, 0b00001000, 0},{0b00010010, 0b00001010, 0b00010101, 0b00001110, 0b00000100, 0},{0b00001010, 0b00001010, 0b00010101, 0b00001110, 0b00000100, 0},{0b00001010, 0b00001010, 0b00010101, 0b00001110, 0b00000100, 0},{0b00010010, 0b00001010, 0b00010101, 0b00001110, 0b00000100, 0},{0b00010010, 0b00010100, 0b00101010, 0b00011100, 0b00001000, 0},{0b00010100, 0b00010100, 0b00101010, 0b00011100, 0b00001000, 0},{0b00100100, 0b00010100, 0b00101010, 0b00011100, 0b00001000, 0},{0b00100100, 0b00101000, 0b01010100, 0b00111000, 0b00010000, 0},{0b00101000, 0b00101000, 0b01010100, 0b00111000, 0b00010000, 0},{0b01001000, 0b00101000, 0b01010100, 0b00111000, 0b00010000, 0},{0b01001000, 0b01010000, 0b10101000, 0b01110000, 0b00100000, 0},{0b01010000, 0b01010000, 0b10101000, 0b01110000, 0b00100000, 8},};

// Auto-generated animation sprite_trick_spin_slow
spriteframe anim_sprite_trick_spin_slow[] = {{0b01010000, 0b01010000, 0b10101000, 0b01110000, 0b00100000, 0},{0b01000000, 0b10101000, 0b01101000, 0b00010000, 0b01101000, 0},{0b00100000, 0b11010000, 0b00111000, 0b11010000, 0b00100000, 0},{0b01101000, 0b00010000, 0b01101000, 0b10101000, 0b01000000, 0},{0b00100000, 0b01110000, 0b10101000, 0b01010000, 0b01010000, 0},{0b00100000, 0b01110000, 0b10101000, 0b01010000, 0b01010000, 0},{0b10110000, 0b01000000, 0b10110000, 0b10101000, 0b00010000, 0},{0b00100000, 0b01011000, 0b11100000, 0b01011000, 0b00100000, 0},{0b00010000, 0b10101000, 0b10110000, 0b01000000, 0b10110000, 0},{0b01010000, 0b01010000, 0b10101000, 0b01110000, 0b00100000, 8},};

// Auto-generated animation sprite_trick_studio
spriteframe anim_sprite_trick_studio[] = {{0b01010000, 0b01010000, 0b10101000, 0b01110000, 0b00100000, 0},{0b01010000, 0b01010000, 0b01100000, 0b01111000, 0b00100000, 0},{0b10010000, 0b01010000, 0b01100000, 0b01110000, 0b00101000, 0},{0b01010000, 0b01010000, 0b01110000, 0b01110000, 0b00100000, 0},{0b01001000, 0b01010000, 0b00110000, 0b01110000, 0b10100000, 0},{0b01010000, 0b01010000, 0b01110000, 0b01110000, 0b00100000, 0},{0b10010000, 0b01010000, 0b01100000, 0b01110000, 0b00101000, 0},{0b01010000, 0b01010000, 0b01110000, 0b01110000, 0b00100000, 0},{0b01001000, 0b01010000, 0b00110000, 0b01110000, 0b10100000, 0},{0b01010000, 0b01010000, 0b01110000, 0b01110000, 0b00100000, 0},{0b10010000, 0b01010000, 0b01100000, 0b01110000, 0b00101000, 0},{0b01010000, 0b01010000, 0b01110000, 0b01110000, 0b00100000, 0},{0b01010000, 0b01010000, 0b10101000, 0b01110000, 0b00100000, 8},};

// Auto-generated animation sprite_wave
spriteframe anim_sprite_wave[] = {{0b01010000, 0b01010000, 0b10101000, 0b01110000, 0b00100000, 0},{0b01010000, 0b01010000, 0b10100000, 0b01111000, 0b00100000, 0},{0b01010000, 0b01010000, 0b10100000, 0b01110000, 0b00101000, 0},{0b01010000, 0b01010000, 0b10100000, 0b01110000, 0b00110000, 0},{0b01010000, 0b01010000, 0b10100000, 0b01110000, 0b00101000, 0},{0b01010000, 0b01010000, 0b10100000, 0b01110000, 0b00110000, 0},{0b01010000, 0b01010000, 0b10100000, 0b01110000, 0b00101000, 0},{0b01010000, 0b01010000, 0b10100000, 0b01110000, 0b00110000, 0},{0b01010000, 0b01010000, 0b10100000, 0b01110000, 0b00101000, 0},{0b01010000, 0b01010000, 0b10100000, 0b01111000, 0b00100000, 0},{0b01010000, 0b01010000, 0b10101000, 0b01110000, 0b00100000, 8},};

const spriteframe * tricks[] = {
		anim_sprite_trick_armdance,
		anim_sprite_trick_ballet,
		anim_sprite_trick_companion,
		anim_sprite_trick_handwave,
		anim_sprite_trick_headroll,
		anim_sprite_trick_headstand,
		anim_sprite_trick_jump,
		anim_sprite_trick_legstand,
		anim_sprite_trick_poof,
		anim_sprite_trick_rocket,
		anim_sprite_trick_russian_slow,
		anim_sprite_trick_shuffle,
		anim_sprite_trick_spin_slow,
		anim_sprite_trick_studio,
		anim_sprite_wave
};

uint8_t sprite_display = 0;
uint8_t sprite_animate = 0;
int8_t sprite_x = 0;
int8_t sprite_y = 0;
spriteframe* sprite_animation;
uint8_t sprite_current_frame;

uint16_t led_values[5] = {65535, 65535, 65535, 65535, 65535};

uint16_t led_zeroes[5] = {0, 0, 0, 0, 0};

uint16_t disp_bit_buffer[BACK_BUFFER_WIDTH] = { 0 };

uint8_t disp_left = 0;
uint8_t disp_top = 0;

volatile uint8_t f_time_loop = 0;
uint8_t print_pixel_len = 0;
uint8_t print_pixel_index = 0;

uint8_t led_text_scrolling = 0;
uint8_t f_animation_done = 0;

volatile uint8_t led_skip_frame_text = 0;
volatile uint8_t led_skip_frame_anim = 0;
volatile uint8_t led_frames_skipped = 0;

#define ANIMATION 0
#define TEXT 1

volatile uint8_t vscroll_to_text = 0;
volatile uint8_t vscroll_to_anim = 0;
volatile uint8_t vertical_mode = TEXT;

void stickman_wave() {
	begin_sprite_animation((spriteframe *) anim_wave, 3);
}

void begin_sprite_animation(spriteframe* animation, uint8_t frameskip) {
	sprite_display = 1;
	sprite_animate = 1;
	sprite_current_frame = 0;
	sprite_x = 0;
	sprite_y = 8;
	sprite_animation = animation;
	led_skip_frame_anim = frameskip;
	f_animation_done = 0;

	if (vertical_mode == TEXT) {
		vscroll_to_anim = 1;
	}
}

void disp_apply_mask(uint16_t mask) {
	for (uint8_t i=0; i<BACK_BUFFER_WIDTH; i++) { // Clear only the sprite area:
		disp_bit_buffer[i] &= mask;
	}
}

//void draw_sprite() {
//	disp_apply_mask(0b0000000011111111);
//	for (uint8_t col = 0; col < 8; col++) {
//		if (sprite_x+col >= 0)
//			disp_bit_buffer[col + sprite_x] |= ((uint16_t) sprite_animation[sprite_current_frame].columns[col]) << (sprite_y);
//	}
//}

void draw_row_major_sprite() {
	disp_apply_mask(0b0000000011111111);
	for (uint8_t row=0; row<5; row++) {
		for (uint8_t col = 0; col < 8; col++) {
			/*
			 * What we need to do:
			 * Put BIT7 of row0 in the bottom left (meaning it becomes BIT4 of col0)
			 * Put BIT6 of row0 in the bottom next-to-left (BIT4 of col1)
			 * Put BIT0 of row0 in the bottom-8 (BIT4 of col7)
			 *
			 * Put BIT7 of row4 in the top left (BIT0 of col0)
			 */

			if (sprite_x+col >= 0) {
				// Start at row0,col0 so we're interested in:
				//  row0 BIT7   for   col0 BIT4
				if (sprite_animation[sprite_current_frame].rows[row] & (1 << (7-col))) {
					// It's a 1, so we need to set col0 BIT4:
					disp_bit_buffer[col+sprite_x] |= (1 << (12-row));
				}
			}
//				disp_bit_buffer[col + sprite_x] |= ((uint16_t) sprite_animation[sprite_current_frame].columns[col]) << (sprite_y);
		}
	}
}

void sprite_next_frame() {
	if (sprite_animation[sprite_current_frame].movement & BIT3) {
		// last frame
		sprite_animate = 0;
		return;
	}
	sprite_current_frame++;
	sprite_x += sprite_animation[sprite_current_frame].movement & 0b111;
}

void led_init() {
#if BADGE_TARGET

	// Setup LED module pins //////////////////////////////////////////////////
	//   bit-banged serial data output:
	//
	// LED_PORT.LED_DATA, LED_CLOCK, LED_LATCH
	//
	GPIO_setAsOutputPin(
			LED_PORT,
			LED_DATA + LED_CLOCK + LED_LATCH // + LED_BLANK
	);

	// BLANK pin (we turn on PWM later as needed):
	GPIO_setAsOutputPin(LED_PORT, LED_BLANK);
	// Shift register input from LED controllers:
//	GPIO_setAsInputPin(LED_PORT, GPIO_PIN6);
	P1DIR &= ~BIT6;
#endif
}

void led_clear() {
	disp_apply_mask(0b0000000000000000);
	led_disp_bit_to_values(disp_left, disp_top);
	led_display_bits(led_values);
}

void led_print_scroll(char* text, uint8_t scroll_on, uint8_t scroll_off, uint8_t frameskip) {
	uint8_t character = 0;
	uint8_t cursor = 0;
	disp_apply_mask(0b1111111100000000); // Clear text area.

	cursor = SCREEN_WIDTH;

	do {
		for (uint16_t i = d3_5ptFontInfo.charInfo[text[character] - d3_5ptFontInfo.startChar].offset; i < d3_5ptFontInfo.charInfo[text[character] - d3_5ptFontInfo.startChar].offset + d3_5ptFontInfo.charInfo[text[character] - d3_5ptFontInfo.startChar].widthBits; i++) {
			disp_bit_buffer[cursor++] = d3_5ptFontInfo.data[i];
			if (cursor == BACK_BUFFER_WIDTH)
				break;
		}
		if (cursor == BACK_BUFFER_WIDTH)
			break; // TODO: Clean this up
		disp_bit_buffer[cursor++] = 0; // gap between letters
		character++;
		if (cursor == BACK_BUFFER_WIDTH)
			break;
	} while (text[character]);

	print_pixel_len = cursor;

	if (!scroll_off) {
		print_pixel_len -= SCREEN_WIDTH; // TODO: Bounds checking
	}

	if (vertical_mode == ANIMATION) {
		vscroll_to_text = 1;
	}

	led_skip_frame_text = frameskip;
	// NOTE: We specifically do NOT set anim_frames_skipped here because
	// if we do, when we're moving from one animation to another with the
	// same frameskip count, the transition looks jerky (which makes sense)
	// We also specifically do NOT write anything to the display buffer
	// at this time, for the same reason.
	disp_left = 0;
	led_text_scrolling = 1;
	f_animation_done = 0;
}

void led_set_rainbow(uint16_t value) {
	value &= 0b01111111111;
	// 0.15 = value.9,
	// 1.15 = value.8
	// 1.7 = value.7
	// 3.15 = value.6
	// 3.7 = value.5
	// 0.0 = value.4
	// 2.8 = value.3
	// 2.0 = value.2
	// 4.8 = value.1
	// 4.0 = value.0
	led_values[0] &= 0b0111111111111110;
	led_values[1] &= 0b0111111101111111;
	led_values[2] &= 0b1111111011111110;
	led_values[3] &= 0b0111111101111111;
	led_values[4] &= 0b1111111011111110;

	led_values[0] |= (value & BIT9)? BIT15 : 0;
	led_values[1] |= (value & BIT8)? BIT15 : 0;
	led_values[1] |= (value & BIT7)? BIT7 : 0;
	led_values[3] |= (value & BIT6)? BIT15 : 0;
	led_values[3] |= (value & BIT5)? BIT7 : 0;
	led_values[0] |= (value & BIT4)? BIT0 : 0;
	led_values[2] |= (value & BIT3)? BIT8 : 0;
	led_values[2] |= (value & BIT2)? BIT0 : 0;
	led_values[4] |= ((value & BIT1)? BIT8 : 0) | ((value & BIT0)? BIT0 : 0);
}

void led_disp_bit_to_values(uint8_t left, uint8_t top) {

	// Clear everything but the rainbows on the end:
	led_values[0] &= 0b1000000000000001;
	for (int i=1; i<5; i++) {
		if (i & 1) {
			led_values[i] &= 0b1000000010000000;
		} else {
			led_values[i] &= 0b0000000100000001;
		}
	}

	int x_offset = 0;

	uint8_t led_segment = 1;
	uint8_t led_index = 0;
	for (uint8_t x=0; x<14; x++) {
		for (uint8_t y=0; y<5; y++) {
			if (y == 0) {
				led_segment = 0;
				x_offset = 1;
			} else {
				if (x<7) {
					led_segment = 1; // LED segment is odd  (left side)
					// left side is x = [0 .. 6]
					// so x_offset is set to skip the leftmost LEDs only,
					// leading to x=0->out=1
					x_offset = 1;
				}
				else {
					led_segment = 2; // LED segment is even (right side)
					// right side is x = [7 .. 13], skipping the far right LED
					// so x_offset is set to make x=7->out=0
					x_offset = -7;
				}
				if (y>2) {
					led_segment+=2;
				}
				if (!(y % 2)) {
					// if Y is even (second row of a controller's segment)
					// then we need the offset to change. i.e. x=0->out=1->out'=9
					// or x=7->out=0->out'=8
					x_offset += 8;
				}
			}

			led_index = x_offset + x;

			// now we need to write to:
			// segment: 	led_segment
			// bit: 		x_offset + x
			if (disp_bit_buffer[(x + left) % BACK_BUFFER_WIDTH] & (1 << ((y + top) % BACK_BUFFER_HEIGHT)))
				led_values[led_segment] |= (1 << (15 - led_index));

		}
	}
}

void led_display_bits(uint16_t* val)
{
	//Set latch to low (should be already)
	GPIO_setOutputLowOnPin(LED_PORT, LED_LATCH);

	uint16_t i;
	uint8_t j;
	for (j=0; j<5; j++) {
		// Iterate over each bit, set data pin, and pulse the clock to send it
		// to the shift register
		for (i = 0; i < 16; i++)  {
			WRITE_IF(LED_PORT, LED_DATA, (val[4-j] & (1 << i)));
			GPIO_pulse(LED_PORT, LED_CLOCK);
		}
	}
	GPIO_pulse(LED_PORT, LED_LATCH);
}

uint8_t led_post()
{
	//Set latch to low (should be already)
	GPIO_setOutputLowOnPin(LED_PORT, LED_LATCH);

	uint16_t i;
	uint8_t j;

	uint16_t test_pattern = 0b1111101010100001;
	uint16_t test_response = 0;
	for (j=0; j<5; j++) { // Fill all the registers with the test pattern.
		for (i = 0; i < 16; i++)  {
			WRITE_IF(LED_PORT, LED_DATA, (test_pattern & (1 << i)));
			GPIO_pulse(LED_PORT, LED_CLOCK);
		}
	}
	// Now read them:
	for (j=0; j<5; j++) {
		// Iterate over each bit, set data pin, and pulse the clock to send it
		// to the shift register
		test_response = 0;
		for (i = 0; i < 16; i++)  {
			test_response |= GPIO_getInputPinValue(GPIO_PORT_P1, GPIO_PIN6) << i;
			WRITE_IF(LED_PORT, LED_DATA, 0);
			GPIO_pulse(LED_PORT, LED_CLOCK);
		}
		if (test_response != test_pattern) {
			return STATUS_FAIL;
		}
	}
	return STATUS_SUCCESS;
}

void led_enable(uint16_t duty_cycle) {
//	led_disable(); // TODO
	GPIO_setAsPeripheralModuleFunctionOutputPin(LED_PORT, LED_BLANK);

	TIMER_A_generatePWM(
		TIMER_A0_BASE,
		TIMER_A_CLOCKSOURCE_ACLK,
		TIMER_A_CLOCKSOURCE_DIVIDER_1,
		LED_PERIOD, // period
		TIMER_A_CAPTURECOMPARE_REGISTER_2,
		TIMER_A_OUTPUTMODE_RESET_SET,
		LED_PERIOD - duty_cycle // duty cycle
	);

	TIMER_A_startCounter(TIMER_A0_BASE, TIMER_A_UP_MODE);

}

void led_anim_init() {
	// In RTC calendar mode (which we're using), PRESCALE_1 starts sourced with
	// a 128 Hz signal.
	RTC_A_definePrescaleEvent(
		RTC_A_BASE,
		RTC_A_PRESCALE_1,
		TIME_LOOP_SCALER
	);

	// Interrupt 8 times per second for animation purposes:
	RTC_A_enableInterrupt(RTC_A_BASE, RTC_A_PRESCALE_TIMER1_INTERRUPT);
}

void led_animate() {
	static uint8_t led_skip_frame;

	// Check to see if we're transitioning display modes:
	if (vscroll_to_text && vertical_mode == ANIMATION) {
		disp_left = 0;
		if (disp_top > 0) {
			disp_top--;
		} else {
			// done...
			vscroll_to_text = 0;
			disp_top = 0; // just in case
			vertical_mode = TEXT;
		}
		led_disp_bit_to_values(disp_left, disp_top);
		led_display_bits(led_values);
		return;
	} else if (vscroll_to_anim && vertical_mode == TEXT) {
		disp_left = 0;
		if (disp_top < 8) {
			disp_top++;
		} else {
			// done...
			vscroll_to_anim = 0;
			disp_top = 8;
			vertical_mode = ANIMATION;
		}
		led_disp_bit_to_values(disp_left, disp_top);
		led_display_bits(led_values);
		return;
	}

	// Check to see if we need to skip this frame.
	led_skip_frame = led_text_scrolling? led_skip_frame_text : led_skip_frame_anim;
	if (led_skip_frame && led_frames_skipped >= led_skip_frame) {
		led_frames_skipped = 0;
		// DISPLAY this frame.
	} else if (led_skip_frame) {
		led_frames_skipped++;
		return;
	}

	if (led_text_scrolling) {
		if (disp_left < print_pixel_len) {
			led_disp_bit_to_values(disp_left, disp_top);
			led_display_bits(led_values);
			disp_left++;
		} else {
			f_animation_done = !sprite_animate;
			led_text_scrolling = 0;
		}
	} else if (sprite_display) {
		draw_row_major_sprite();
		if (sprite_animate) {
			sprite_next_frame();
			if (!sprite_animate) {
				f_animation_done = 1;
			}
		}
		led_disp_bit_to_values(disp_left, disp_top);
		led_display_bits(led_values);
	}
}

void led_disable( void )
{
	GPIO_setAsOutputPin(
			LED_PORT,
			LED_BLANK
	);

	GPIO_setOutputHighOnPin(LED_PORT, LED_BLANK);
}

inline void led_toggle( void ) {
	GPIO_toggleOutputOnPin(LED_PORT, LED_BLANK);
}
