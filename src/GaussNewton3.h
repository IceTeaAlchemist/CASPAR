//
//  Academic License - for use in teaching, academic research, and meeting
//  course requirements at degree granting institutions only.  Not for
//  government, commercial, or other organizational use.
//
//  GaussNewton3.h
//
//  Code generation for function 'GaussNewton3'
//


#ifndef GAUSSNEWTON3_H
#define GAUSSNEWTON3_H

// Include files
#include "rtwtypes.h"
#include "coder_array.h"
#include <cstddef>
#include <cstdlib>

// Function Declarations
extern void GaussNewton3(const coder::array<double, 1U> &x, const coder::array<
  double, 2U> &y, const double beta0[3], const double lb[3], const double ub[3],
  double coeff[3], double *iter);

#endif

// End of code generation (GaussNewton3.h)
