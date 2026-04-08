/*
 * waveform_gen.c
 * LabVIEW Call Library Function Node compatible waveform generator
 * Target: NI Linux RT (x86-64, e.g. cRIO-9040)
 *
 * Fixed sine:  freq_start == freq_stop
 * Sine sweep:  freq_start != freq_stop (linear instantaneous frequency ramp)
 *
 * LabVIEW CLFN terminal mapping:
 *   Parameter 1:  amp         double   (pass by value)
 *   Parameter 2:  freq_start  double   (pass by value)
 *   Parameter 3:  freq_stop   double   (pass by value)
 *   Parameter 4:  dt          double   (pass by value)
 *   Parameter 5:  length_pts  int32_t  (pass by value)  -- cast to int
 *   Parameter 6:  out_array   double*  (pass as C array pointer)  -- Adapt to match CLFN configuration
 *
 * Return type: void (no return value terminal needed)
 *
 * CLFN Configuration Notes (LabVIEW block diagram):
 *   - Calling convention: C
 *   - Parameters 1-5: Numeric, Value
 *   - Parameter 6: Array Data Pointer (double *), pass as Pointer to Value
 *   - Wire an I32 "length_pts" control to terminal 5 so LabVIEW allocates the
 *     output array before calling; pre-allocate a 1D DBL array of the same size
 *     and pass its data pointer to parameter 6.
 *   - Alternatively use the LVArray / DSNewHClr pattern if you prefer LabVIEW
 *     to own the allocation — see accompanying README.
 */

#include <stdint.h>
#include <math.h>
#include <string.h>

#ifdef _WIN32
  #define EXPORT __declspec(dllexport)
#else
  #define EXPORT __attribute__((visibility("default")))
#endif

EXPORT void generatewaveform(
    double   amp,
    double   freq_start,
    double   freq_stop,
    double   dt,
    int32_t  length_pts,
    double  *out_array
) {
    if (length_pts <= 0 || out_array == NULL) return;

    double phase = 0.0;
    int n = (int)length_pts;

    for (int i = 0; i < n; i++) {
        double f_inst;

        if (freq_start == freq_stop || n == 1) {
            f_inst = freq_start;
        } else {
            double frac = (double)i / (double)(n - 1);
            f_inst = freq_start + (freq_stop - freq_start) * frac;
        }

        phase += 2.0 * M_PI * f_inst * dt;
        out_array[i] = amp * sin(phase);
    }
}


/* ---------- smoke test (excluded from .so, runs only when built as executable) ---------- */
#ifdef WAVEFORM_SMOKE_TEST
#include <stdio.h>
#include <stdlib.h>

static int smoke(const char *name, double amp, double f0, double f1,
                 double dt, int n, double expected_peak) {
    double *buf = (double *)malloc(n * sizeof(double));
    generatewaveform(amp, f0, f1, dt, (int32_t)n, buf);

    double peak = 0.0;
    for (int i = 0; i < n; i++) {
        double v = fabs(buf[i]);
        if (v > peak) peak = v;
    }
    free(buf);

    int ok = peak > expected_peak * 0.5;
    printf("%s: peak=%.4f  expected>%.4f  %s\n", name, peak, expected_peak * 0.5, ok ? "PASS" : "FAIL");
    return ok;
}

int main(void) {
    int all = 1;
    all &= smoke("fixed_10hz",  2.0, 10.0,  10.0,  0.001, 1000, 1.5);
    all &= smoke("up_sweep",    1.0,  5.0,  50.0,  0.001, 2000, 0.5);
    all &= smoke("down_sweep",  1.5, 100.0, 20.0, 0.0005, 3000, 0.8);
    printf("OVERALL: %s\n", all ? "PASS" : "FAIL");
    return all ? 0 : 1;
}
#endif
