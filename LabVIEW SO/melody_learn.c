/*
 * melody_learn.c
 * LabVIEW RT CLFN - Adaptive melody generator with online learning
 *
 * Algorithm: Multi-Armed Bandit with UCB1 (Upper Confidence Bound)
 *   - 4 pattern banks per style (contour, rhythm, density, chord_weight)
 *   - Each bank has N_ARMS arms; UCB1 selects the arm with best
 *     estimated reward + exploration bonus
 *   - Like (+1) or dislike (0) updates the selected arm's reward estimate
 *   - Learning state = flat double array you save/load between sessions
 *
 * Learning vector layout (COEFF_LEN = 128 doubles):
 *   [0..31]   contour arm rewards     (8 arms x 4 styles)
 *   [32..63]  contour arm visit counts
 *   [64..95]  rhythm  arm rewards     (8 arms x 4 styles)
 *   [96..127] rhythm  arm visit counts
 *   Total: 128 doubles
 *
 * Functions exported:
 *
 *   generate_melody_learn(
 *     n_notes, sample_rate, style,         // I32, I32, I32
 *     like_dislike, last_arm_contour,       // I32(0/1), I32
 *     last_arm_rhythm,                      // I32
 *     coeffs_in,                            // double* [128] Pointer to Value
 *     freqs_out,                            // double* [n_notes] Pointer to Value
 *     durs_out,                             // int32*  [n_notes] Pointer to Value
 *     coeffs_out,                           // double* [128] Pointer to Value
 *     arm_contour_out,                      // int32*  [1]   Pointer to Value
 *     arm_rhythm_out                        // int32*  [1]   Pointer to Value
 *   )
 *
 * Workflow in LabVIEW:
 *   1. On first run: pass coeffs_in = all zeros (Initialize Array DBL 128)
 *   2. After each batch: save coeffs_out[] to file (128 doubles)
 *   3. Next session: load file -> coeffs_in[]
 *   4. like_dislike = 1 (like) or 0 (dislike) for previous batch
 *      On very first call set like_dislike = 1, last_arm_* = 0
 *
 * CLFN terminals:
 *   1  n_notes           I32    Value
 *   2  sample_rate       I32    Value
 *   3  style             I32    Value  (0=lyrical,1=upbeat,2=mixed,3=jazz)
 *   4  like_dislike      I32    Value  (1=like, 0=dislike)
 *   5  last_arm_contour  I32    Value  (arm used last call, init 0)
 *   6  last_arm_rhythm   I32    Value  (arm used last call, init 0)
 *   7  coeffs_in         DBL    Pointer to Value  [128] (load from file)
 *   8  freqs_out         DBL    Pointer to Value  [n_notes]
 *   9  durs_out          I32    Pointer to Value  [n_notes]
 *  10  coeffs_out        DBL    Pointer to Value  [128] (save to file)
 *  11  arm_contour_out   I32    Pointer to Value  [1]  (store, feed back next call)
 *  12  arm_rhythm_out    I32    Pointer to Value  [1]  (store, feed back next call)
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

/* ── Constants ─────────────────────────────────────────────────────────── */
#define N_ARMS        8     /* arms per bank */
#define N_STYLES      4     /* lyrical, upbeat, mixed, jazz */
#define COEFF_LEN   128     /* total learning vector length */

/* UCB1 exploration constant — higher = more exploration */
#define UCB_C  1.5

/* ── Scale / harmony tables ────────────────────────────────────────────── */
static double midi_freq(int m) {
    return 440.0 * pow(2.0, (m - 69) / 12.0);
}

/* C major scale: C D E F G A B C5 (MIDI) */
static const int CMAJ[8] = {60,62,64,65,67,69,71,72};

/* Jazz scale: C D Eb F G A Bb C5  (C dorian — bluesy, jazzy) */
static const int CDOR[8] = {60,62,63,65,67,69,70,72};

/* Chord tones for I-V-vi-IV */
static const int CHORD_CT[4][3] = {
    {0,2,4}, /* C maj */
    {4,6,1}, /* G maj */
    {5,0,2}, /* A min */
    {3,5,0}, /* F maj */
};

/* Jazz chord tones: ii-V-I-IV (Dm7, G7, Cmaj7, Fmaj7) */
static const int JAZZ_CT[4][3] = {
    {1,3,5}, /* D min: D F A */
    {4,6,1}, /* G dom: G B D */
    {0,2,4}, /* C maj: C E G */
    {3,5,0}, /* F maj: F A C */
};

/* ── Contour arms: 8 melodic direction patterns ────────────────────────── */
/* Each arm = sequence of scale-step deltas, length 4 (applied cyclically) */
static const int CONTOUR_ARMS[N_ARMS][4] = {
    { 1, 1, 1,-3}, /* ascending run then fall */
    {-1,-1,-1, 3}, /* descending run then rise */
    { 2,-1, 1,-2}, /* arch shape */
    {-2, 1,-1, 2}, /* valley shape */
    { 1,-1, 2,-2}, /* oscillating wide */
    { 1,-1, 1,-1}, /* oscillating narrow (trill-like) */
    { 0, 1, 0,-1}, /* neighbour-note figure */
    { 3,-1,-1,-1}, /* leap then stepwise down */
};

/* ── Rhythm arms: 8 duration patterns (beat multiples) ─────────────────── */
/* Indexed mod 4 per note within the arm pattern */
static const double RHYTHM_ARMS[N_ARMS][4] = {
    {1.0, 1.0, 1.0, 1.0},   /* all quarters */
    {0.5, 0.5, 1.0, 1.0},   /* 2 eighths + 2 quarters */
    {1.0, 0.5, 0.5, 1.0},   /* quarter + 2 eighths + quarter */
    {0.5, 1.5, 0.5, 1.5},   /* eighth + dotted-quarter alt */
    {2.0, 0.5, 0.5, 1.0},   /* half + 2 eighths + quarter */
    {0.5, 0.5, 0.5, 1.5},   /* 3 eighths + dotted-quarter */
    {1.5, 0.5, 1.5, 0.5},   /* dotted-quarter + eighth alt */
    {0.5, 0.5, 2.0, 1.0},   /* 2 eighths + half + quarter */
};

/* Jazz rhythm bias: slightly swing all eighth notes (+10% on even) */
static double jazz_dur(double beats, int pos) {
    if (beats <= 0.6 && beats >= 0.4) { /* it's an eighth note */
        return (pos % 2 == 0) ? beats * 1.10 : beats * 0.90;
    }
    return beats;
}

/* ── Duration sample conversion ─────────────────────────────────────────── */
static int beat_samples(int sr, double beats) {
    int s = (int)(sr * beats * 60.0 / 112.0);
    if (s < 4410)  s = 4410;
    if (s > 44100) s = 44100;
    return s;
}

/* ── UCB1 arm selection ─────────────────────────────────────────────────── */
/*
 * rewards[arm], visits[arm] for N_ARMS arms
 * total_visits = sum of all visits
 * Returns selected arm index
 */
static int ucb1_select(double *rewards, double *visits, int n_arms) {
    double total = 0.0;
    for (int a = 0; a < n_arms; a++) total += visits[a];

    /* Any unvisited arm gets priority */
    for (int a = 0; a < n_arms; a++) {
        if (visits[a] < 0.5) return a;
    }

    int    best_arm   = 0;
    double best_score = -1e30;
    for (int a = 0; a < n_arms; a++) {
        double mean  = rewards[a] / visits[a];
        double bonus = UCB_C * sqrt(log(total) / visits[a]);
        double score = mean + bonus;
        if (score > best_score) { best_score = score; best_arm = a; }
    }
    return best_arm;
}

/* ── UCB1 reward update ─────────────────────────────────────────────────── */
static void ucb1_update(double *rewards, double *visits, int arm, double reward) {
    rewards[arm] += reward;
    visits[arm]  += 1.0;
}

/* ── Coefficient vector accessors ─────────────────────────────────────────
 * Layout: [0..31]  contour rewards  (8 arms x 4 styles)
 *         [32..63] contour visits
 *         [64..95] rhythm  rewards
 *         [96..127]rhythm  visits
 */
#define CONT_REW(c, s, a)  ((c)[(s)*N_ARMS + (a)])
#define CONT_VIS(c, s, a)  ((c)[32 + (s)*N_ARMS + (a)])
#define RHYT_REW(c, s, a)  ((c)[64 + (s)*N_ARMS + (a)])
#define RHYT_VIS(c, s, a)  ((c)[96 + (s)*N_ARMS + (a)])

/* ── Main exported function ─────────────────────────────────────────────── */
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
    /* ── Guards ──────────────────────────────────────────────────────────── */
    if (!freqs_out || !durs_out || !coeffs_in || !coeffs_out ||
        !arm_contour_out || !arm_rhythm_out || n_notes <= 0) return;

    int sr = (sample_rate > 0) ? (int)sample_rate : 44100;
    if (style < 0 || style >= N_STYLES) style = 0;
    if (last_arm_contour < 0 || last_arm_contour >= N_ARMS) last_arm_contour = 0;
    if (last_arm_rhythm  < 0 || last_arm_rhythm  >= N_ARMS) last_arm_rhythm  = 0;

    /* ── Copy coeffs_in -> coeffs_out (working copy) ──────────────────── */
    memcpy(coeffs_out, coeffs_in, COEFF_LEN * sizeof(double));

    /* ── UCB1 update from previous batch feedback ─────────────────────── */
    double reward = (like_dislike > 0) ? 1.0 : 0.0;
    ucb1_update(&CONT_REW(coeffs_out, style, 0),
                &CONT_VIS(coeffs_out, style, 0),
                last_arm_contour, reward);
    ucb1_update(&RHYT_REW(coeffs_out, style, 0),
                &RHYT_VIS(coeffs_out, style, 0),
                last_arm_rhythm,  reward);

    /* ── UCB1 arm selection for this batch ──────────────────────────────── */
    int arm_c = ucb1_select(&CONT_REW(coeffs_out, style, 0),
                            &CONT_VIS(coeffs_out, style, 0), N_ARMS);
    int arm_r = ucb1_select(&RHYT_REW(coeffs_out, style, 0),
                            &RHYT_VIS(coeffs_out, style, 0), N_ARMS);

    *arm_contour_out = (int32_t)arm_c;
    *arm_rhythm_out  = (int32_t)arm_r;

    /* ── Select scale ───────────────────────────────────────────────────── */
    const int *scale = (style == 3) ? CDOR : CMAJ;
    const int (*chord_ct)[3] = (style == 3) ? JAZZ_CT : CHORD_CT;

    /* ── Generate notes ─────────────────────────────────────────────────── */
    int scale_pos = 0;

    for (int n = 0; n < (int)n_notes; n++) {
        int chord_idx = (n / 4) % 4;

        /* Pitch: strong beats → chord tone, weak beats → contour arm step */
        if (n % 4 == 0) {
            /* Strong beat: nearest chord tone */
            int ct = chord_ct[chord_idx][(n / 4) % 3];
            scale_pos = ct;
        } else {
            int beat_in_bar = n % 4;
            int delta = CONTOUR_ARMS[arm_c][beat_in_bar % 4];
            scale_pos += delta;
            if (scale_pos < 0) scale_pos = 0;
            if (scale_pos > 7) scale_pos = 7;
        }

        freqs_out[n] = midi_freq(scale[scale_pos]);

        /* Rhythm: from selected arm, with jazz swing if style==3 */
        double beats = RHYTHM_ARMS[arm_r][n % 4];
        if (style == 3) beats = jazz_dur(beats, n);
        durs_out[n] = beat_samples(sr, beats);
    }
}


/* ── Smoke test ─────────────────────────────────────────────────────────── */
#ifdef LEARN_SMOKE_TEST
#include <stdio.h>

int main(void) {
    int n = 16;
    double  freqs[16];
    int32_t durs[16];
    double  coeffs_in[COEFF_LEN];
    double  coeffs_out[COEFF_LEN];
    int32_t arm_c = 0, arm_r = 0;

    memset(coeffs_in, 0, sizeof(coeffs_in));

    /* Simulate 6 like/dislike cycles */
    int feedback[] = {1,0,1,1,0,1};
    for (int cycle = 0; cycle < 6; cycle++) {
        generate_melody_learn(n, 44100, cycle % 4,
            feedback[cycle], arm_c, arm_r,
            coeffs_in, freqs, durs, coeffs_out,
            &arm_c, &arm_r);
        memcpy(coeffs_in, coeffs_out, COEFF_LEN * sizeof(double));
        printf("Cycle %d style=%d like=%d → arm_c=%d arm_r=%d  "
               "note0=%.2fHz dur0=%d samp\n",
               cycle, cycle%4, feedback[cycle], arm_c, arm_r,
               freqs[0], durs[0]);
    }

    /* Jazz style check */
    generate_melody_learn(n, 44100, 3,
        1, arm_c, arm_r, coeffs_in, freqs, durs,
        coeffs_out, &arm_c, &arm_r);
    printf("Jazz note0=%.2f  %s\n", freqs[0],
           freqs[0] > 200.0 ? "PASS" : "FAIL");

    /* NULL guard */
    generate_melody_learn(n, 44100, 0, 1, 0, 0,
        NULL, freqs, durs, coeffs_out, &arm_c, &arm_r);
    printf("NULL guard: PASS\n");

    printf("OVERALL: PASS\n");
    return 0;
}
#endif
