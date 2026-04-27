import numpy as np


def _normalize_waveform_type(waveform_type):
    text = str(waveform_type).strip().lower()
    aliases = {
        "sine": "sine",
        "sin": "sine",
        "square": "square",
        "sq": "square",
        "triangle": "triangle",
        "tri": "triangle",
        "saw": "saw",
        "sawtooth": "saw",
        "noise": "noise",
        "random": "noise",
        "chirp": "chirp",
        "sweep": "chirp",
        "step": "step",
        "dc": "step",
    }
    return aliases.get(text, "sine")



def _clamp_frequency(frequency_hz):
    if frequency_hz < 0.0:
        return 0.0
    return float(frequency_hz)



def generate_waveform(amplitude, freq_start, freq_stop, dt, waveform_type, length_pts):
    """
    Generate a LabVIEW Python Node compatible waveform or sweep.

    Inputs
    - amplitude: DBL -> float
    - freq_start: DBL -> float, Hz
    - freq_stop: DBL -> float, Hz
    - dt: DBL -> float, seconds
    - waveform_type: String/Ring -> str
    - length_pts: Numeric -> float/int, cast to int internally

    Output
    - signal: 1D Array of DBL -> list[float]

    Notes
    - Returns a plain Python list of float for LabVIEW compatibility.
    - Uses phase accumulation for frequency sweeps to avoid distortion.
    - No default arguments are used.
    """
    amplitude = float(amplitude)
    freq_start = _clamp_frequency(float(freq_start))
    freq_stop = _clamp_frequency(float(freq_stop))
    dt = float(dt)
    n_pts = int(length_pts)
    waveform_name = _normalize_waveform_type(waveform_type)

    if n_pts <= 0:
        return []

    if dt <= 0.0:
        return [0.0 for _ in range(n_pts)]

    if n_pts == 1:
        frequencies = np.array([freq_start], dtype=float)
    else:
        frequencies = np.linspace(freq_start, freq_stop, n_pts, dtype=float)

    signal = []
    phase = 0.0

    for i in range(n_pts):
        f_now = float(frequencies[i])
        phase += 2.0 * np.pi * f_now * dt

        if waveform_name == "sine":
            y = amplitude * np.sin(phase)

        elif waveform_name == "square":
            y = amplitude if np.sin(phase) >= 0.0 else -amplitude

        elif waveform_name == "triangle":
            phase_wrapped = np.mod(phase, 2.0 * np.pi)
            y = amplitude * (2.0 / np.pi) * np.arcsin(np.sin(phase_wrapped))

        elif waveform_name == "saw":
            phase_wrapped = np.mod(phase, 2.0 * np.pi)
            y = amplitude * ((phase_wrapped / np.pi) - 1.0)

        elif waveform_name == "noise":
            y = amplitude * (2.0 * np.random.random() - 1.0)

        elif waveform_name == "chirp":
            y = amplitude * np.sin(phase)

        elif waveform_name == "step":
            midpoint = n_pts // 2
            y = 0.0 if i < midpoint else amplitude

        else:
            y = amplitude * np.sin(phase)

        signal.append(float(y))

    return signal


if __name__ == "__main__":
    tests = [
        (1.0, 100.0, 500.0, 0.0001, "Sine", 30000),
        (2.0, 10.0, 10.0, 0.001, "square", 1000),
        (1.5, 5.0, 50.0, 0.001, "triangle", 2000),
        (1.0, 20.0, 20.0, 0.001, "saw", 1000),
        (0.5, 0.0, 0.0, 0.001, "noise", 500),
    ]

    for index, args in enumerate(tests, start=1):
        result = generate_waveform(*args)
        is_list = isinstance(result, list)
        correct_len = len(result) == int(args[5])
        all_float = all(isinstance(value, float) for value in result)
        passed = is_list and correct_len and all_float
        print(f"Test {index}: {'PASS' if passed else 'FAIL'} | type={args[4]} | len={len(result)}")
