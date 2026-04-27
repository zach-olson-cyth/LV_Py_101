import numpy as np


def generate_waveform(amp, freq_start, freq_stop, dt, waveform_type, length_pts):
    n = int(length_pts)

    if n <= 0:
        return []

    amp = float(amp)
    freq_start = float(freq_start)
    freq_stop = float(freq_stop)
    dt = float(dt)
    waveform_type = str(waveform_type)

    y = []
    phase = 0.0

    if n == 1:
        freqs = [freq_start]
    else:
        freqs = np.linspace(freq_start, freq_stop, n)

    for f in freqs:
        phase += 2.0 * np.pi * f * dt

        if waveform_type.lower() == "sine":
            sample = amp * np.sin(phase)
        elif waveform_type.lower() == "square":
            sample = amp if np.sin(phase) >= 0.0 else -amp
        elif waveform_type.lower() == "triangle":
            sample = amp * (2.0 / np.pi) * np.arcsin(np.sin(phase))
        elif waveform_type.lower() == "saw":
            sample = amp * (2.0 * ((phase / (2.0 * np.pi)) % 1.0) - 1.0)
        else:
            sample = amp * np.sin(phase)

        y.append(float(sample))

    return y


if __name__ == "__main__":
    tests = [
        (1.0, 10.0, 50.0, 0.001, "Sine", 1000),
        (2.0, 5.0, 25.0, 0.001, "Square", 1000),
        (1.5, 20.0, 20.0, 0.0005, "Triangle", 2000),
    ]

    for i, args in enumerate(tests, 1):
        result = generate_waveform(*args)
        ok = (
            isinstance(result, list)
            and len(result) == int(args[5])
            and all(isinstance(v, float) for v in result)
        )
        print(f"Test {i}: {'PASS' if ok else 'FAIL'}")