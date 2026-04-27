import numpy as np


def generatewaveform(amp, freq_start, freq_stop, dt, waveform_type, length_pts):
    """
    LabVIEW Python Node compatible waveform generator.

    Fixed sine:
        freq_start == freq_stop

    Sine sweep:
        freq_start != freq_stop

    Inputs
    ------
    amp : float
    freq_start : float
    freq_stop : float
    dt : float
    waveform_type : str
    length_pts : float

    Returns
    -------
    list[float]
        Plain Python list for LabVIEW 1D DBL array output.
    """
    n = int(length_pts)

    if n <= 0:
        return []

    amp = float(amp)
    freq_start = float(freq_start)
    freq_stop = float(freq_stop)
    dt = float(dt)
    waveform_type = str(waveform_type).strip()

    if dt <= 0.0:
        raise ValueError("dt must be > 0")

    if waveform_type.lower() != "sine":
        raise ValueError("Only 'Sine' waveform_type is supported")

    y = []
    phase = 0.0

    if n == 1:
        return [0.0]

    for i in range(n):
        if freq_start == freq_stop:
            f_inst = freq_start
        else:
            frac = i / (n - 1)
            f_inst = freq_start + (freq_stop - freq_start) * frac

        phase += 2.0 * np.pi * f_inst * dt
        y.append(float(amp * np.sin(phase)))

    return y


if __name__ == "__main__":
    tests = [
        ("fixed_10hz", 2.0, 10.0, 10.0, 0.001, "Sine", 1000),
        ("up_sweep", 1.0, 5.0, 50.0, 0.001, "Sine", 2000),
        ("down_sweep", 1.5, 100.0, 20.0, 0.0005, "Sine", 3000),
    ]

    all_passed = True

    for name, amp, f0, f1, dt, wtype, n in tests:
        try:
            result = generatewaveform(amp, f0, f1, dt, wtype, n)
            ok = (
                isinstance(result, list)
                and len(result) == int(n)
                and all(isinstance(v, float) for v in result)
            )
            print(f"{name}: {'PASS' if ok else 'FAIL'}")
            if not ok:
                all_passed = False
        except Exception:
            print(f"{name}: FAIL")
            all_passed = False

    print("OVERALL:", "PASS" if all_passed else "FAIL")