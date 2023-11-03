//
//  Academic License - for use in teaching, academic research, and meeting
//  course requirements at degree granting institutions only.  Not for
//  government, commercial, or other organizational use.
//
//  GaussNewton4.cpp
//
//  Code generation for function 'GaussNewton4'
//


// Include files
#include "GaussNewton4.h"
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

//
//  function [coeff, iter] = GaussNewton4(x,y,beta0)

void GaussNewton4(const coder::array<double, 2U> &x, const coder::array<double,
                  2U> &y, const double beta0[4], double coeff[4], double *iter)
{
  static const double b_y[16] = { 1.0E-5, 0.0, 0.0, 0.0, 0.0, 1.0E-5, 0.0, 0.0,
    0.0, 0.0, 1.0E-5, 0.0, 0.0, 0.0, 0.0, 1.0E-5 };

  coder::array<double, 2U> d;
  coder::array<double, 2U> j;
  double C[16];
  double da[4];
  int b_iter;
  int coffset;
  int i;
  int jy;
  signed char ipiv[4];
  boolean_T exitg1;

  //  Set function tolerance, this is the maximum change for convergence
  //  condition.
  // 'GaussNewton4:4' tol =0.0001;
  //  Set the maximum number of iterations the program will take.
  // 'GaussNewton4:6' iter_max = 26;
  // 'GaussNewton4:7' n = length(x);
  //  Set up the initial guess.
  // 'GaussNewton4:9' coeff = beta0;
  coeff[0] = beta0[0];
  coeff[1] = beta0[1];
  coeff[2] = beta0[2];
  coeff[3] = beta0[3];

  // 'GaussNewton4:10' f = zeros(n,1);
  // 'GaussNewton4:11' j = zeros(n,4);
  j.set_size(x.size(1), 4);
  jy = x.size(1) << 2;
  for (i = 0; i < jy; i++) {
    j[i] = 0.0;
  }

  // 'GaussNewton4:12' d = zeros(1,n);
  d.set_size(1, x.size(1));
  jy = x.size(1);
  for (i = 0; i < jy; i++) {
    d[i] = 0.0;
  }

  //  Begin looping.
  // 'GaussNewton4:16' for iter = 1:iter_max
  *iter = 1.0;
  b_iter = 0;
  exitg1 = false;
  while ((!exitg1) && (b_iter < 26)) {
    double a;
    double b;
    double c;
    int b_j;
    int boffset;
    int k;
    *iter = static_cast<double>(b_iter) + 1.0;

    //  Set coefficient guesses.
    // 'GaussNewton4:18' a = coeff(1);
    a = coeff[0];

    // 'GaussNewton4:19' b = coeff(2);
    b = coeff[1];

    // 'GaussNewton4:20' c = coeff(3);
    c = coeff[2];

    // 'GaussNewton4:21' e = coeff(4);
    //  Create the Jacobian.
    // 'GaussNewton4:23' for i = 1:n
    i = x.size(1);
    for (jy = 0; jy < i; jy++) {
      double a_tmp;
      double j_tmp;
      double j_tmp_tmp;

      // 'GaussNewton4:24' f(i,1) = a*exp(-(x(i) - b)^2/(2*c^2))+e;
      a_tmp = x[jy] - b;

      // 'GaussNewton4:25' j(i,1) = exp(-(x(i) - b)^2/(2*c^2));
      j_tmp_tmp = c * c;
      j_tmp = 2.0 * j_tmp_tmp;
      j[jy] = std::exp(-(a_tmp * a_tmp) / j_tmp);

      // 'GaussNewton4:26' j(i,2) = a*(x(i)-b)*exp(-(x(i) - b)^2/(2*c^2))/c^2;
      j[jy + j.size(0)] = a * a_tmp * std::exp(-(a_tmp * a_tmp) / j_tmp) /
        j_tmp_tmp;

      // 'GaussNewton4:27' j(i,3) = a*(x(i)-b)^2*exp(-(x(i) - b)^2/(2*c^2))/c^3; 
      j[jy + j.size(0) * 2] = a * (a_tmp * a_tmp) * std::exp(-(a_tmp * a_tmp) /
        j_tmp) / rt_powd_snf(c, 3.0);

      // 'GaussNewton4:28' j(i,4) = 1;
      j[jy + j.size(0) * 3] = 1.0;

      // 'GaussNewton4:29' d(i) = y(i) - f(i);
      d[jy] = y[jy] - (a * std::exp(-(a_tmp * a_tmp) / j_tmp) + coeff[3]);
    }

    //  Damp the Jacobian to avoid poor conditioning and add it.
    // 'GaussNewton4:32' da = (j'*j + 0.00001*eye(size(j'*j)))\(j'*d');
    jy = j.size(0);
    da[0] = 0.0;
    da[1] = 0.0;
    da[2] = 0.0;
    da[3] = 0.0;
    for (k = 0; k < jy; k++) {
      da[0] += j[k] * d[k];
      da[1] += j[j.size(0) + k] * d[k];
      da[2] += j[(j.size(0) << 1) + k] * d[k];
      da[3] += j[3 * j.size(0) + k] * d[k];
    }

    jy = j.size(0);
    for (b_j = 0; b_j < 4; b_j++) {
      coffset = b_j << 2;
      boffset = b_j * j.size(0);
      C[coffset] = 0.0;
      C[coffset + 1] = 0.0;
      C[coffset + 2] = 0.0;
      C[coffset + 3] = 0.0;
      for (k = 0; k < jy; k++) {
        a = j[boffset + k];
        C[coffset] += j[k] * a;
        C[coffset + 1] += j[j.size(0) + k] * a;
        C[coffset + 2] += j[(j.size(0) << 1) + k] * a;
        C[coffset + 3] += j[3 * j.size(0) + k] * a;
      }
    }

    for (i = 0; i < 16; i++) {
      C[i] += b_y[i];
    }

    ipiv[0] = 1;
    ipiv[1] = 2;
    ipiv[2] = 3;
    for (b_j = 0; b_j < 3; b_j++) {
      int b_tmp;
      int ix;
      int mmj_tmp;
      mmj_tmp = 2 - b_j;
      b_tmp = b_j * 5;
      boffset = b_tmp + 2;
      jy = 4 - b_j;
      coffset = 0;
      ix = b_tmp;
      a = std::abs(C[b_tmp]);
      for (k = 2; k <= jy; k++) {
        ix++;
        b = std::abs(C[ix]);
        if (b > a) {
          coffset = k - 1;
          a = b;
        }
      }

      if (C[b_tmp + coffset] != 0.0) {
        if (coffset != 0) {
          jy = b_j + coffset;
          ipiv[b_j] = static_cast<signed char>(jy + 1);
          a = C[b_j];
          C[b_j] = C[jy];
          C[jy] = a;
          a = C[b_j + 4];
          C[b_j + 4] = C[jy + 4];
          C[jy + 4] = a;
          a = C[b_j + 8];
          C[b_j + 8] = C[jy + 8];
          C[jy + 8] = a;
          a = C[b_j + 12];
          C[b_j + 12] = C[jy + 12];
          C[jy + 12] = a;
        }

        i = (b_tmp - b_j) + 4;
        for (jy = boffset; jy <= i; jy++) {
          C[jy - 1] /= C[b_tmp];
        }
      }

      jy = b_tmp + 4;
      coffset = b_tmp;
      for (k = 0; k <= mmj_tmp; k++) {
        a = C[jy];
        if (C[jy] != 0.0) {
          ix = b_tmp + 1;
          i = coffset + 6;
          boffset = (coffset - b_j) + 8;
          for (int ijA = i; ijA <= boffset; ijA++) {
            C[ijA - 1] += C[ix] * -a;
            ix++;
          }
        }

        jy += 4;
        coffset += 4;
      }
    }

    if (ipiv[0] != 1) {
      a = da[0];
      da[0] = da[ipiv[0] - 1];
      da[ipiv[0] - 1] = a;
    }

    if (ipiv[1] != 2) {
      a = da[1];
      da[1] = da[ipiv[1] - 1];
      da[ipiv[1] - 1] = a;
    }

    if (ipiv[2] != 3) {
      a = da[2];
      da[2] = da[ipiv[2] - 1];
      da[ipiv[2] - 1] = a;
    }

    if (da[0] != 0.0) {
      for (jy = 2; jy < 5; jy++) {
        da[jy - 1] -= da[0] * C[jy - 1];
      }
    }

    if (da[1] != 0.0) {
      for (jy = 3; jy < 5; jy++) {
        da[jy - 1] -= da[1] * C[jy + 3];
      }
    }

    if (da[2] != 0.0) {
      for (jy = 4; jy < 5; jy++) {
        da[3] -= da[2] * C[11];
      }
    }

    if (da[3] != 0.0) {
      da[3] /= C[15];
      for (jy = 0; jy < 3; jy++) {
        da[jy] -= da[3] * C[jy + 12];
      }
    }

    if (da[2] != 0.0) {
      da[2] /= C[10];
      for (jy = 0; jy < 2; jy++) {
        da[jy] -= da[2] * C[jy + 8];
      }
    }

    if (da[1] != 0.0) {
      da[1] /= C[5];
      for (jy = 0; jy < 1; jy++) {
        da[0] -= da[1] * C[4];
      }
    }

    if (da[0] != 0.0) {
      da[0] /= C[0];
    }

    //  Iterate.
    // 'GaussNewton4:34' coeff = coeff +da';
    coeff[0] += da[0];
    coeff[1] += da[1];
    coeff[2] += da[2];
    coeff[3] += da[3];

    //  Invoke boundary conditions.
    //      if(coeff(1) < lb(1))
    //          coeff(1) = lb(1);
    //      end
    //      if(coeff(2) < lb(2))
    //          coeff(2) = lb(2);
    //      end
    //      if(coeff(3) < lb(3))
    //          coeff(3) = lb(3);
    //      end
    //      if(coeff(1) > ub(1))
    //          coeff(1) = ub(1);
    //      end
    //      if(coeff(2) > ub(2))
    //          coeff(2) = ub(2);
    //      end
    //      if(coeff(3) > ub(3))
    //          coeff(3) = ub(3);
    //      end
    //  Break on convergence.
    // 'GaussNewton4:56' if(abs(da(1))<tol && abs(da(2))<tol && abs(da(3))<tol && abs(da(4)) < tol) 
    if ((std::abs(da[0]) < 0.0001) && (std::abs(da[1]) < 0.0001) && (std::abs
         (da[2]) < 0.0001) && (std::abs(da[3]) < 0.0001)) {
      //         disp(out);
      //        disp('Method has converged.');
      exitg1 = true;
    } else {
      b_iter++;
    }
  }
}

// End of code generation (GaussNewton4.cpp)
