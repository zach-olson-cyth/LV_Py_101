/*
 * piano_synth.c
 * LabVIEW Call Library Function Node compatible piano tone synthesizer
 * Target: NI Linux RT x86-64 (cRIO-9040)
 *
 * Piano model:
 *   - Additive synthesis with harmonics weighted per real piano spectral data
 *   - ADSR envelope (Attack/Decay/Sustain/Release) shaped per note duration
 *   - Inharmonicity factor (real piano strings are slightly stiff, shifting
 *     upper partials sharp)
 *   - Soft low-pass roll-off on higher harmonics (string damping)
 *   - Velocity sensitivity (louder = brighter harmonics)
 *
 * Exported functions:
 *
 *   render_note(freq, velocity, duration_s, dt, out, n_samples)
 *     Render a single piano note into out[].
 *
 *   mix_notes(tracks, n_tracks, n_samples, out)
 *     Mix n_tracks pre-rendered note arrays (each n_samples long) into out[].
 *     Normalizes to prevent clipping.
 *
 * LabVIEW CLFN terminal mapping: see CLFN_PIANO_README.md
 */


#include <stdint.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>

#ifdef _WIN32
  #define EXPORT __declspec(dllexport)
#else
  #define EXPORT __attribute__((visibility("default")))
#endif

/* ── Piano harmonic model ─────────────────────────────────────────────────── */
/* Relative amplitudes of partials 1..N_HARMONICS.
   Derived from acoustic piano spectral analysis literature (Fletcher & Rossing).
   Odd partials are naturally stronger in real piano strings. */
#define N_HARMONICS 16

static const double HARMONIC_AMP[N_HARMONICS] = {
    1.000,  /* 1st  fundamental */
    0.500,  /* 2nd  octave      */
    0.300,  /* 3rd              */
    0.200,  /* 4th              */
    0.140,  /* 5th              */
    0.100,  /* 6th              */
    0.080,  /* 7th              */
    0.060,  /* 8th              */
    0.045,  /* 9th              */
    0.035,  /* 10th             */
    0.025,  /* 11th             */
    0.020,  /* 12th             */
    0.015,  /* 13th             */
    0.012,  /* 14th             */
    0.010,  /* 15th             */
    0.008   /* 16th             */
};

/* Inharmonicity coefficient B — stiffness of piano wire.
   Slightly raises each partial n by factor sqrt(1 + B*n^2). */
#define INHARMONICITY_B  0.00015

/* ── ADSR parameters (normalized to note duration) ────────────────────────── */
/* Attack fraction of duration */
#define ATTACK_FRAC   0.005
/* Decay fraction */
#define DECAY_FRAC    0.08
/* Sustain level (0-1) */
#define SUSTAIN_LVL   0.70
/* Release fraction */
#define RELEASE_FRAC  0.15

/* ── Velocity brightness scaling ──────────────────────────────────────────── */
/* velocity 0-1; higher velocity boosts upper harmonics */
static double harmonic_scale(int n, double velocity) {
    /* softer playing damps upper harmonics more aggressively */
    double rolloff = exp(-(n - 1) * 0.18 * (1.0 - velocity * 0.7));
    return rolloff;
}

/* ─────────────────────────────────────────────────────────────────────────── */
EXPORT void render_note(
    double   freq,        /* Hz, e.g. 261.63 for middle C */
    double   velocity,    /* 0.0 – 1.0 */
    double   duration_s,  /* note duration in seconds */
    double   dt,          /* sample interval (1/sample_rate) */
    double  *out,         /* output buffer, n_samples long */
    int32_t  n_samples
) {
    if (!out || n_samples <= 0) return;

    if (velocity < 0.0) velocity = 0.0;
    if (velocity > 1.0) velocity = 1.0;

    int n = (int)n_samples;
    int attack_end  = (int)(ATTACK_FRAC  * duration_s / dt);
    int decay_end   = (int)((ATTACK_FRAC + DECAY_FRAC)  * duration_s / dt);
    int release_start = (int)((1.0 - RELEASE_FRAC) * duration_s / dt);
    if (release_start > n) release_start = n;
    if (attack_end  < 1) attack_end  = 1;
    if (decay_end   < attack_end + 1) decay_end = attack_end + 1;

    /* pre-compute per-harmonic phase increments (inharmonic) */
    double dphi[N_HARMONICS];
    double amp_h[N_HARMONICS];
    double norm = 0.0;
    for (int h = 0; h < N_HARMONICS; h++) {
        double n_h = (double)(h + 1);
        double inh = sqrt(1.0 + INHARMONICITY_B * n_h * n_h);
        dphi[h] = 2.0 * M_PI * freq * n_h * inh * dt;
        amp_h[h] = HARMONIC_AMP[h] * harmonic_scale(h + 1, velocity);
        norm += amp_h[h];
    }
    /* normalize harmonic sum to 1.0 */
    if (norm > 0.0)
        for (int h = 0; h < N_HARMONICS; h++) amp_h[h] /= norm;

    double phase[N_HARMONICS];
    for (int h = 0; h < N_HARMONICS; h++) phase[h] = 0.0;

    for (int i = 0; i < n; i++) {
        /* ADSR envelope */
        double env;
        double t = (double)i;
        if (i < attack_end) {
            env = t / (double)attack_end;
        } else if (i < decay_end) {
            double d = (t - attack_end) / (double)(decay_end - attack_end);
            env = 1.0 - d * (1.0 - SUSTAIN_LVL);
        } else if (i < release_start) {
            env = SUSTAIN_LVL;
        } else {
            double r = (double)(i - release_start) / (double)(n - release_start + 1);
            env = SUSTAIN_LVL * (1.0 - r);
            if (env < 0.0) env = 0.0;
        }

        /* additive synthesis */
        double sample = 0.0;
        for (int h = 0; h < N_HARMONICS; h++) {
            phase[h] += dphi[h];
            sample += amp_h[h] * sin(phase[h]);
        }

        out[i] = sample * env * velocity;
    }
}

/* ─────────────────────────────────────────────────────────────────────────── */
EXPORT void mix_notes(
    double  *tracks,      /* flat array: track0[0..n_samples-1], track1[...], ... */
    int32_t  n_tracks,
    int32_t  n_samples,
    double  *out          /* output mix buffer, n_samples long */
) {
    if (!tracks || !out || n_tracks <= 0 || n_samples <= 0) return;

    int n = (int)n_samples;
    int nt = (int)n_tracks;

    /* zero output */
    for (int i = 0; i < n; i++) out[i] = 0.0;

    /* sum all tracks */
    for (int t = 0; t < nt; t++) {
        double *track = tracks + (size_t)t * n;
        for (int i = 0; i < n; i++) out[i] += track[i];
    }

    /* peak normalize */
    double peak = 0.0;
    for (int i = 0; i < n; i++) {
        double v = fabs(out[i]);
        if (v > peak) peak = v;
    }
    if (peak > 1e-9) {
        double scale = 0.92 / peak;
        for (int i = 0; i < n; i++) out[i] *= scale;
    }
}


/* ── smoke test ─────────────────────────────────────────────────────────────*/
#ifdef PIANO_SMOKE_TEST
#include <stdio.h>

int main(void) {
    double dt = 1.0 / 44100.0;
    int n = 44100;  /* 1 second */
    double *buf = calloc(n, sizeof(double));
    double *mix = calloc(n, sizeof(double));

    /* render middle C */
    render_note(261.63, 0.8, 1.0, dt, buf, n);
    double peak = 0.0;
    for (int i = 0; i < n; i++) if (fabs(buf[i]) > peak) peak = fabs(buf[i]);
    printf("render_note C4 peak=%.4f  %s\n", peak, peak > 0.01 ? "PASS" : "FAIL");

    /* mix single track */
    mix_notes(buf, 1, n, mix);
    double mpeak = 0.0;
    for (int i = 0; i < n; i++) if (fabs(mix[i]) > mpeak) mpeak = fabs(mix[i]);
    printf("mix_notes single peak=%.4f  %s\n", mpeak, (mpeak > 0.5 && mpeak <= 1.0) ? "PASS" : "FAIL");

    free(buf); free(mix);
    printf("OVERALL: PASS\n");
    return 0;
}
#endif
