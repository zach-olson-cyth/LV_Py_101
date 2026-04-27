import numpy as np


def generate_waveform(amp, freq_start, freq_stop, dt, waveform_type, length_pts):
    """
    LabVIEW Python Node compatible waveform generator.

    Terminal order must match LabVIEW block diagram:
    1. amp           -> DBL -> float
    2. freq_start    -> DBL -> float
    3. freq_stop     -> DBL -> float
    4. dt            -> DBL -> float
    5. waveform_type -> String -> str
    6. length_pts    -> DBL -> float, cast to int

    Returns:
        list[float] : waveform samples
    """

    amp = float(amp)
    freq_start = float(freq_start)
    freq_stop = float(freq_stop)
    dt = float(dt)
    waveform_type = str(waveform_type).strip().lower()
    n = int(length_pts)

    if n <= 0:
        return []

    y = []
    phase = 0.0

    if n == 1:
        freqs = [freq_start]
    else:
        freqs = np.linspace(freq_start, freq_stop, n)

    for f in freqs:
        phase += 2.0 * np.pi * float(f) * dt

        if waveform_type == "sine":
            sample = amp * np.sin(phase)

        elif waveform_type == "square":
            sample = amp if np.sin(phase) >= 0.0 else -amp

        elif waveform_type == "triangle":
            sample = amp * (2.0 / np.pi) * np.arcsin(np.sin(phase))

        elif waveform_type == "saw":
            sample = amp * (2.0 * ((phase / (2.0 * np.pi)) % 1.0) - 1.0)

        else:
            sample = 0.0

        y.append(float(sample))

    return y


if __name__ == "__main__":
    tests = [
        (1.0, 10.0, 10.0, 0.001, "Sine", 1000),
        (2.0, 5.0, 20.0, 0.001, "Square", 500),
        (1.5, 2.0, 10.0, 0.002, "Triangle", 400),
    ]

    for i, args in enumerate(tests, 1):
        result = generate_waveform(*args)
        ok = isinstance(result, list) and len(result) == int(args[5]) and all(isinstance(v, float) for v in result)
        print(f"Test {i}: {'PASS' if ok else 'FAIL'}")
