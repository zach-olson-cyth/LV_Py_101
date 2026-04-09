#include <stdint.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>

#ifdef _WIN32
  #define EXPORT __declspec(dllexport)
#else
  #define EXPORT __attribute__((visibility("default")))
#endif

#define N_ARMS    8
#define N_STYLES  6      /* 0=lyrical 1=upbeat 2=mixed 3=jazz 4=rock_band 5=beat */
#define COEFF_LEN 256
#define UCB_C     1.5

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static double midi_freq(int m) {
    return 440.0 * pow(2.0, (m - 69) / 12.0);
}

/* ── Scales ──────────────────────────────────────────────────────── */
static const int CMAJ[8] = {60,62,64,65,67,69,71,72};   /* C major */
static const int CDOR[8] = {60,62,63,65,67,69,70,72};   /* C Dorian */
static const int APEN[8] = {57,60,62,64,67,69,72,76};   /* A pentatonic minor */
/* Drum "scale": MIDI pitches for kick/snare/hat/tom-hi/tom-mid/tom-lo/crash/ride */
static const int DRUM[8] = {36,40,43,47,50,53,57,60};

/* ── Chord tone indices into each scale ─────────────────────────── */
static const int CHORD_CT[4][3]  = {{0,2,4},{4,6,1},{5,0,2},{3,5,0}};
static const int JAZZ_CT[4][3]   = {{1,3,5},{4,6,1},{0,2,4},{3,5,0}};
static const int ROCK_CT[4][3]   = {{0,2,4},{6,1,3},{5,0,2},{6,1,4}};
/* Beat: anchor positions = kick(0), snare(1), hihat(2), kick(0) */
static const int BEAT_CT[4][3]   = {{0,1,2},{0,1,2},{0,1,2},{0,1,2}};

/* ── Contour arms (melodic deltas for styles 0-4) ────────────────── */
static const int CONTOUR_ARMS[N_ARMS][4] = {
    { 1, 1, 1,-3},
    {-1,-1,-1, 3},
    { 2,-1, 1,-2},
    {-2, 1,-1, 2},
    { 1,-1, 2,-2},
    { 1,-1, 1,-1},
    { 0, 1, 0,-1},
    { 3,-1,-1,-1},
};

/* ── Drum contour arms: absolute drum positions (not deltas) ───────
   Each row is 4 absolute indices into DRUM[8] for a fill pattern   */
static const int DRUM_ARMS[N_ARMS][4] = {
    {0, 1, 0, 1},   /* kick-snare basic */
    {0, 2, 1, 2},   /* kick-hat-snare-hat */
    {0, 2, 2, 1},   /* kick-hat-hat-snare */
    {3, 4, 5, 1},   /* tom fill hi-mid-lo-snare */
    {0, 0, 1, 2},   /* double kick-snare-hat */
    {0, 1, 2, 6},   /* kick-snare-hat-crash */
    {0, 2, 1, 7},   /* kick-hat-snare-ride */
    {0, 3, 1, 4},   /* kick-tom-snare-tom */
};

/* ── Rhythm arms ─────────────────────────────────────────────────── */
static const double RHYTHM_ARMS[N_ARMS][4] = {
    {1.0, 1.0, 1.0, 1.0},
    {0.5, 0.5, 1.0, 1.0},
    {1.0, 0.5, 0.5, 1.0},
    {0.5, 1.5, 0.5, 1.5},
    {2.0, 0.5, 0.5, 1.0},
    {0.5, 0.5, 0.5, 1.5},
    {1.5, 0.5, 1.5, 0.5},
    {0.5, 0.5, 2.0, 1.0},
};

/* Rock: 16th-note heavy rhythm arms */
static const double ROCK_RHYTHM[N_ARMS][4] = {
    {0.25,0.25,0.25,0.25},
    {0.25,0.25,0.50,0.25},
    {0.50,0.25,0.25,0.25},
    {0.25,0.50,0.25,0.25},
    {0.375,0.125,0.25,0.25},
    {0.25,0.375,0.125,0.25},
    {0.25,0.25,0.375,0.125},
    {0.375,0.25,0.125,0.25},
};

/* Beat: drum groove rhythm arms at 120 BPM */
static const double BEAT_RHYTHM[N_ARMS][4] = {
    {1.0, 0.5, 1.0, 0.5},
    {0.5, 0.5, 0.5, 0.5},
    {0.25,0.25,0.25,0.25},
    {0.75,0.25,0.75,0.25},
    {0.5, 1.0, 0.5, 1.0},
    {0.25,0.50,0.25,0.50},
    {1.0, 0.25,0.25,0.50},
    {0.25,0.75,0.25,0.75},
};

/* ── Timing helpers ──────────────────────────────────────────────── */
static double jazz_dur(double beats, int pos) {
    if (beats >= 0.4 && beats <= 0.6)
        return (pos % 2 == 0) ? beats * 1.10 : beats * 0.90;
    return beats;
}

static int beat_samples(int sr, double beats, int style) {
    double bpm = (style == 4) ? 132.0 : (style == 5) ? 120.0 : 112.0;
    int s = (int)(sr * beats * 60.0 / bpm);
    if (s < 2205)  s = 2205;
    if (s > 44100) s = 44100;
    return s;
}

/* ── UCB1 ────────────────────────────────────────────────────────── */
static int ucb1_select(double *rewards, double *visits, int n_arms) {
    double total = 0.0;
    for (int a = 0; a < n_arms; a++) total += visits[a];
    for (int a = 0; a < n_arms; a++)
        if (visits[a] < 0.5) return a;
    int best = 0;
    double best_score = -1e30;
    for (int a = 0; a < n_arms; a++) {
        double score = rewards[a]/visits[a] + UCB_C*sqrt(log(total)/visits[a]);
        if (score > best_score) { best_score = score; best = a; }
    }
    return best;
}

static void ucb1_update(double *rewards, double *visits, int arm, double reward) {
    if (arm < 0 || arm >= N_ARMS) return;
    rewards[arm] += reward;
    visits[arm]  += 1.0;
}

/* ── Coefficient layout (256 doubles, 6 styles x 8 arms x 4 banks) ─
   Bank 0: contour rewards  [0..47]
   Bank 1: contour visits   [48..95]
   Bank 2: rhythm  rewards  [96..143]
   Bank 3: rhythm  visits   [144..191]
   [192..255] reserved                                               */
#define CONT_REW(c,s,a) ((c)[(s)*N_ARMS+(a)])
#define CONT_VIS(c,s,a) ((c)[48+(s)*N_ARMS+(a)])
#define RHYT_REW(c,s,a) ((c)[96+(s)*N_ARMS+(a)])
#define RHYT_VIS(c,s,a) ((c)[144+(s)*N_ARMS+(a)])

/* ── Main exported function ──────────────────────────────────────── */
EXPORT void generate_melody_learn(
    int32_t  n_notes,
    int32_t  sample_rate,
    int32_t  style,
    int32_t  like_dislike,
    int32_t  last_arm_contour,
    int32_t  last_arm_rhythm,
    double  *coeffs_in,
    double  *freqs_out,
    int32_t *durs_out,
    double  *coeffs_out,
    int32_t *arm_contour_out,
    int32_t *arm_rhythm_out
) {
    if (!freqs_out || !durs_out || !coeffs_in || !coeffs_out ||
        !arm_contour_out || !arm_rhythm_out || n_notes <= 0) return;

    int sr = (sample_rate > 0) ? (int)sample_rate : 44100;
    if (style < 0 || style >= N_STYLES) style = 0;  /* safe clamp */

    int lc = (last_arm_contour >= 0 && last_arm_contour < N_ARMS) ? last_arm_contour : -1;
    int lr = (last_arm_rhythm  >= 0 && last_arm_rhythm  < N_ARMS) ? last_arm_rhythm  : -1;

    memcpy(coeffs_out, coeffs_in, COEFF_LEN * sizeof(double));

    /* UCB1 update on previous arms */
    if (lc >= 0 && CONT_VIS(coeffs_out, style, lc) > 0.0) {
        double r = (like_dislike > 0) ? 1.0 : 0.0;
        ucb1_update(&CONT_REW(coeffs_out,style,0),
                    &CONT_VIS(coeffs_out,style,0), lc, r);
    }
    if (lr >= 0 && RHYT_VIS(coeffs_out, style, lr) > 0.0) {
        double r = (like_dislike > 0) ? 1.0 : 0.0;
        ucb1_update(&RHYT_REW(coeffs_out,style,0),
                    &RHYT_VIS(coeffs_out,style,0), lr, r);
    }

    /* Select arms */
    int arm_c = ucb1_select(&CONT_REW(coeffs_out,style,0),
                             &CONT_VIS(coeffs_out,style,0), N_ARMS);
    int arm_r = ucb1_select(&RHYT_REW(coeffs_out,style,0),
                             &RHYT_VIS(coeffs_out,style,0), N_ARMS);

    /* Pending credit */
    CONT_VIS(coeffs_out,style,arm_c) += 0.5;
    RHYT_VIS(coeffs_out,style,arm_r) += 0.5;

    *arm_contour_out = (int32_t)arm_c;
    *arm_rhythm_out  = (int32_t)arm_r;

    /* ── Note generation ── */
    int scale_pos = 0;

    for (int n = 0; n < (int)n_notes; n++) {
        int chord_idx = (n / 4) % 4;

        if (style == 5) {
            /* BEAT: use DRUM_ARMS absolute positions directly */
            int drum_pos = DRUM_ARMS[arm_c][n % 4];
            if (drum_pos < 0) drum_pos = 0;
            if (drum_pos > 7) drum_pos = 7;
            freqs_out[n] = midi_freq(DRUM[drum_pos]);

            double beats = BEAT_RHYTHM[arm_r][n % 4];
            durs_out[n]  = beat_samples(sr, beats, 5);

        } else if (style == 4) {
            /* ROCK BAND: pentatonic */
            if (n % 4 == 0) {
                int ct = ROCK_CT[chord_idx][(n/4) % 3];
                scale_pos = (ct >= 0 && ct < 8) ? ct : 0;
            } else {
                scale_pos += CONTOUR_ARMS[arm_c][n % 4];
                if (scale_pos < 0) scale_pos = 0;
                if (scale_pos > 7) scale_pos = 7;
            }
            freqs_out[n] = midi_freq(APEN[scale_pos]);
            double beats = ROCK_RHYTHM[arm_r][n % 4];
            durs_out[n]  = beat_samples(sr, beats, 4);

        } else if (style == 3) {
            /* JAZZ: C Dorian */
            if (n % 4 == 0) {
                int ct = JAZZ_CT[chord_idx][(n/4) % 3];
                scale_pos = (ct >= 0 && ct < 8) ? ct : 0;
            } else {
                scale_pos += CONTOUR_ARMS[arm_c][n % 4];
                if (scale_pos < 0) scale_pos = 0;
                if (scale_pos > 7) scale_pos = 7;
            }
            freqs_out[n] = midi_freq(CDOR[scale_pos]);
            double beats = jazz_dur(RHYTHM_ARMS[arm_r][n % 4], n);
            durs_out[n]  = beat_samples(sr, beats, 3);

        } else {
            /* LYRICAL / UPBEAT / MIXED: C major */
            if (n % 4 == 0) {
                int ct = CHORD_CT[chord_idx][(n/4) % 3];
                scale_pos = (ct >= 0 && ct < 8) ? ct : 0;
            } else {
                scale_pos += CONTOUR_ARMS[arm_c][n % 4];
                if (scale_pos < 0) scale_pos = 0;
                if (scale_pos > 7) scale_pos = 7;
            }
            freqs_out[n] = midi_freq(CMAJ[scale_pos]);
            double beats = RHYTHM_ARMS[arm_r][n % 4];
            durs_out[n]  = beat_samples(sr, beats, style);
        }
    }
}

/* ── Smoke test ──────────────────────────────────────────────────── */
#ifdef LEARN_SMOKE_TEST
#include <stdio.h>
int main(void) {
    int n = 16;
    double freqs[16]; int32_t durs[16];
    double ci[256], co[256];
    int32_t ac=0, ar=0;
    memset(ci, 0, sizeof(ci));
    int all_ok = 1;

    const char *names[] = {"lyrical","upbeat","mixed","jazz","rock_band","beat"};
    for (int s = 0; s < 6; s++) {
        memset(ci, 0, sizeof(ci));
        ac = 0; ar = 0;
        for (int call = 0; call < 3; call++) {
            generate_melody_learn(n, 44100, s, 1, ac, ar, ci, freqs, durs, co, &ac, &ar);
            memcpy(ci, co, 256*sizeof(double));
        }
        int bad = 0;
        for (int i = 0; i < n; i++) if (freqs[i] < 30.0 || durs[i] < 100) bad++;
        printf("style %d (%s): arm_c=%d arm_r=%d  bad=%d  f[0]=%.2f f[1]=%.2f f[2]=%.2f  %s\n",
               s, names[s], ac, ar, bad, freqs[0], freqs[1], freqs[2],
               bad==0 ? "PASS" : "FAIL");
        if (bad) all_ok = 0;
    }
    printf("OVERALL: %s\n", all_ok ? "PASS" : "FAIL");
    return all_ok ? 0 : 1;
}
#endif
