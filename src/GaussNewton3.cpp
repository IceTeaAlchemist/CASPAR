//
//  Academic License - for use in teaching, academic research, and meeting
//  course requirements at degree granting institutions only.  Not for
//  government, commercial, or other organizational use.
//
//  GaussNewton3.cpp
//
//  Code generation for function 'GaussNewton3'
//


// Include files
#include "GaussNewton3.h"
#include "rt_nonfinite.h"
#include "coder_array.h"
#include "rt_nonfinite.h"
#include <cmath>

// Function Declarations
static double rt_powd_snf(double u0, double u1);

// Function Definitions
static double rt_powd_snf(double u0, double u1)
{
  double y;
  if (rtIsNaN(u0) || rtIsNaN(u1)) {
    y = rtNaN;
  } else {
    double d;
    double d1;
    d = std::abs(u0);
    d1 = std::abs(u1);
    if (rtIsInf(u1)) {
      if (d == 1.0) {
        y = 1.0;
      } else if (d > 1.0) {
        if (u1 > 0.0) {
          y = rtInf;
        } else {
          y = 0.0;
        }
      } else if (u1 > 0.0) {
        y = 0.0;
      } else {
        y = rtInf;
      }
    } else if (d1 == 0.0) {
      y = 1.0;
    } else if (d1 == 1.0) {
      if (u1 > 0.0) {
        y = u0;
      } else {
        y = 1.0 / u0;
      }
    } else if (u1 == 2.0) {
      y = u0 * u0;
    } else if ((u1 == 0.5) && (u0 >= 0.0)) {
      y = std::sqrt(u0);
    } else if ((u0 < 0.0) && (u1 > std::floor(u1))) {
      y = rtNaN;
    } else {
      y = std::pow(u0, u1);
    }
  }

  return y;
}

void GaussNewton3(const coder::array<double, 1U> &x, const coder::array<double,
                  2U> &y, const double beta0[3], const double lb[3], const
                  double ub[3], double coeff[3], double *iter)
{
  static const double c_y[9] = { 0.0001, 0.0, 0.0, 0.0, 0.0001, 0.0, 0.0, 0.0,
    0.0001 };

  coder::array<double, 2U> d;
  coder::array<double, 2U> j;
  double C[9];
  double b_y[3];
  double a;
  int b_iter;
  int coffset;
  int r1;
  bool exitg1;

  //  Set function tolerance, this is the maximum change for convergence
  //  condition.
  //  Set the maximum number of iterations the program will take.
  //  Set up the initial guess.
  coeff[0] = beta0[0];
  coeff[1] = beta0[1];
  coeff[2] = beta0[2];
  j.set_size(x.size(0), 3);
  r1 = x.size(0) * 3;
  for (coffset = 0; coffset < r1; coffset++) {
    j[coffset] = 0.0;
  }

  d.set_size(1, x.size(0));
  r1 = x.size(0);
  for (coffset = 0; coffset < r1; coffset++) {
    d[coffset] = 0.0;
  }

  //  Begin looping.
  *iter = 1.0;
  b_iter = 0;
  exitg1 = false;
  while ((!exitg1) && (b_iter < 26)) {
    double b;
    double c;
    int boffset;
    int rtemp;
    *iter = static_cast<double>(b_iter) + 1.0;

    //  Set coefficient guesses.
    a = coeff[0];
    b = coeff[1];
    c = coeff[2];

    //  Create the Jacobian.
    coffset = x.size(0);
    for (r1 = 0; r1 < coffset; r1++) {
      double a_tmp;
      double j_tmp;
      double j_tmp_tmp;
      a_tmp = x[r1] - b;
      j_tmp_tmp = c * c;
      j_tmp = 2.0 * j_tmp_tmp;
      j[r1] = std::exp(-(a_tmp * a_tmp) / j_tmp);
      j[r1 + j.size(0)] = a * a_tmp * std::exp(-(a_tmp * a_tmp) / j_tmp) /
        j_tmp_tmp;
      j[r1 + j.size(0) * 2] = a * (a_tmp * a_tmp) * std::exp(-(a_tmp * a_tmp) /
        j_tmp) / rt_powd_snf(c, 3.0);
      d[r1] = y[r1] - a * std::exp(-(a_tmp * a_tmp) / j_tmp);
    }

    //  Damp the Jacobian to avoid poor conditioning and add it.
    r1 = j.size(0);
    b_y[0] = 0.0;
    b_y[1] = 0.0;
    b_y[2] = 0.0;
    for (rtemp = 0; rtemp < r1; rtemp++) {
      b_y[0] += j[rtemp] * d[rtemp];
      b_y[1] += j[j.size(0) + rtemp] * d[rtemp];
      b_y[2] += j[(j.size(0) << 1) + rtemp] * d[rtemp];
    }

    r1 = j.size(0);
    for (int b_j = 0; b_j < 3; b_j++) {
      coffset = b_j * 3;
      boffset = b_j * j.size(0);
      C[coffset] = 0.0;
      C[coffset + 1] = 0.0;
      C[coffset + 2] = 0.0;
      for (rtemp = 0; rtemp < r1; rtemp++) {
        a = j[boffset + rtemp];
        C[coffset] += j[rtemp] * a;
        C[coffset + 1] += j[j.size(0) + rtemp] * a;
        C[coffset + 2] += j[(j.size(0) << 1) + rtemp] * a;
      }
    }

    for (coffset = 0; coffset < 9; coffset++) {
      C[coffset] += c_y[coffset];
    }

    r1 = 0;
    coffset = 1;
    boffset = 2;
    a = std::abs(C[0]);
    b = std::abs(C[1]);
    if (b > a) {
      a = b;
      r1 = 1;
      coffset = 0;
    }

    if (std::abs(C[2]) > a) {
      r1 = 2;
      coffset = 1;
      boffset = 0;
    }

    C[coffset] /= C[r1];
    C[boffset] /= C[r1];
    C[coffset + 3] -= C[coffset] * C[r1 + 3];
    C[boffset + 3] -= C[boffset] * C[r1 + 3];
    C[coffset + 6] -= C[coffset] * C[r1 + 6];
    C[boffset + 6] -= C[boffset] * C[r1 + 6];
    if (std::abs(C[boffset + 3]) > std::abs(C[coffset + 3])) {
      rtemp = coffset;
      coffset = boffset;
      boffset = rtemp;
    }

    C[boffset + 3] /= C[coffset + 3];
    C[boffset + 6] -= C[boffset + 3] * C[coffset + 6];
    a = b_y[coffset] - b_y[r1] * C[coffset];
    b = ((b_y[boffset] - b_y[r1] * C[boffset]) - a * C[boffset + 3]) / C[boffset
      + 6];
    a -= b * C[coffset + 6];
    a /= C[coffset + 3];
    c = ((b_y[r1] - b * C[r1 + 6]) - a * C[r1 + 3]) / C[r1];

    //  Iterate.
    coeff[0] += c;
    coeff[1] += a;
    coeff[2] += b;

    //  Invoke boundary conditions.
    if (coeff[0] < lb[0]) {
      coeff[0] = lb[0];
    }

    if (coeff[1] < lb[1]) {
      coeff[1] = lb[1];
    }

    if (coeff[2] < lb[2]) {
      coeff[2] = lb[2];
    }

    if (coeff[0] > ub[0]) {
      coeff[0] = ub[0];
    }

    if (coeff[1] > ub[1]) {
      coeff[1] = ub[1];
    }

    if (coeff[2] > ub[2]) {
      coeff[2] = ub[2];
    }

    //  Break on convergence.
    if ((std::abs(c) < 0.001) && (std::abs(a) < 0.001) && (std::abs(b) < 0.001))
    {
      //         disp(out);
      //        disp('Method has converged.');
      exitg1 = true;
    } else {
      b_iter++;
    }
  }
}

// End of code generation (GaussNewton3.cpp)
