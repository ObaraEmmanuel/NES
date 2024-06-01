# pragma once

/* this holds the data required to update samples thru a filter */
typedef struct {
    double a0, a1, a2, a3, a4;
    double x1, x2, y1, y2;
}Biquad;

/* filter types */
enum {
    LPF, /* low pass filter */
    HPF, /* High pass filter */
    BPF, /* band pass filter */
    NOTCH, /* Notch Filter */
    PEQ, /* Peaking band EQ filter */
    LSH, /* Low shelf filter */
    HSH /* High shelf filter */
};

double biquad(double sample, Biquad *b);

void biquad_init(Biquad* b, int type,
                   double dbGain, /* gain of filter */
                   double freq, /* center frequency */
                   double srate, /* sampling rate */
                   double bandwidth); /* bandwidth in octaves */
