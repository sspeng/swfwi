/* This file is automatically generated. DO NOT EDIT! */

#ifndef _sf_mksinc_h
#define _sf_mksinc_h


void mksinc (float d     /* fractional distance to interpolation point; 0.0<=d<=1.0 */, 
	     int lsinc   /* length of sinc approximation; lsinc%2==0 and lsinc<=20 */, 
	     float *sinc /* [lsinc] array containing interpolation coefficients */);
/*< Compute least-squares optimal sinc interpolation coefficients.

The coefficients are a least-squares-best approximation to the ideal
sinc function for frequencies from zero up to a computed maximum
frequency.  For a given interpolator length, lsinc, mksinc computes
the maximum frequency, fmax (expressed as a fraction of the nyquist
frequency), using the following empirically derived relation (from
a Western Geophysical Technical Memorandum by Ken Larner):

	fmax = min(0.066+0.265*log(lsinc),1.0)

Note that fmax increases as lsinc increases, up to a maximum of 1.0.
Use the coefficients to interpolate a uniformly-sampled function y(i) 
as follows:

            lsinc-1
    y(i+d) =  sum  sinc[j]*y(i+j+1-lsinc/2)
              j=0

Interpolation error is greatest for d=0.5, but for frequencies less
than fmax, the error should be less than 1.0 percent. >*/

#endif