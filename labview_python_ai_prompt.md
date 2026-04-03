# LabVIEW Python Node — AI Prompt Specification
## Signal Generator · well_model.py · cRIO Integration · v2.0

> **Purpose:** Paste any prompt in this document into **ChatGPT, Claude, Perplexity, Grok, or Gemini**
> to generate, debug, or extend any Python function designed for the LabVIEW Python Node.
> Replace all `[PLACEHOLDERS]` with your specifics before sending.
>
> **Works in:** Any AI chat interface — no special formatting required. All prompts are plain text.
>
> **v2.0 updates:** Full data-type mapping (scalars, arrays, clusters, strings), 1D array return fix,
> image-based VI analysis prompt, waveform debugging checklist, numpy install guide.

---

## How to Use This Document

1. Find the prompt section that matches your task (generate, debug, extend, fix, etc.)
2. Copy the entire code block
3. Replace all `[PLACEHOLDERS]` with your actual values
4. Paste into ChatGPT, Claude, Perplexity, or any other AI
5. The AI will produce LabVIEW-compatible Python code that follows all rules below

---

## Quick Copy Prompts

### 1. Generate a New LabVIEW-Compatible Python Function

```
Write a Python function for the LabVIEW Python Node with these requirements:

FUNCTION NAME: [your_function_name]
PURPOSE: [what it computes, e.g., "simulate oil well pressure and flow rate"]
INPUTS: [list each parameter — name, LabVIEW type, Python type, units, typical range]
OUTPUTS: [list each return value — name, LabVIEW type, Python type, units]
PHYSICS/LOGIC: [equations or algorithm to implement]
STATE: [any values that carry between calls, e.g., "P_wh and Q persist as inputs"]

Rules the function MUST follow:
1. Scalars: use plain Python float, int, or str — no np.float32, no np.int64
2. 1D Arrays: return as plain Python list[float] or list[int] — NOT numpy arrays, NOT tuples wrapping lists
3. Multiple outputs: return as a tuple — (scalar1, scalar2, list1)
4. No default argument values — LabVIEW passes all arguments positionally
5. No module-level mutable state — all state passed in and returned out
6. import statements at top of file only (numpy is allowed)
7. Include a __main__ block with a 5-step smoke test that prints PASS/FAIL

LabVIEW Python Node context:
- LabVIEW 2020+ on Windows, 64-bit Python 3.9–3.14
- Scalar DBL/I32/String → Python float/int/str
- 1D Array of DBL → Python list[float]  (return type terminal = empty 1D DBL Array constant)
- Cluster → Python tuple (return type terminal = LabVIEW Cluster constant)
- All input terminals must be wired — no defaults are used
- numpy is allowed; pandas, scipy, tensorflow are NOT available in the LabVIEW Python environment
```

---

### 2. Analyze a Front Panel or Block Diagram Image

```
I am attaching a screenshot of a LabVIEW [front panel / block diagram].
Analyze the image and do the following:

1. LIST all input controls you can see (name, LabVIEW control type, likely data type)
2. LIST all output indicators you can see (name, LabVIEW indicator type, likely data type)
3. INFER the Python function signature that would match this VI, using these type mappings:
   - Numeric control (DBL/SGL/I32) → float or int
   - String control / ring / enum → str
   - Boolean / LED → int (1 or 0)
   - 1D Array indicator or waveform graph → list[float]
   - Cluster → tuple of matching Python types
   - dt control → float (sample interval in seconds)
   - N Samples / num_samples control → float (cast to int inside Python)
4. WRITE a complete Python function with the correct signature, docstring, and __main__ smoke test
5. NOTE any dt, N samples, or sample-rate controls that affect output array length
6. WARN if dt appears too large for the given frequency (Nyquist: dt < 1 / (2 * max_freq))

Apply all LabVIEW Python Node rules:
- No default args
- Plain Python types only (float, int, str, list, tuple)
- No numpy arrays as return values — use list[float]
- No dicts
- Return tuple for multiple outputs; return list[float] for a single array output
- Include a __main__ smoke test with at least 3 test cases
```

---

### 3. Debug an Existing Function (Paste Your Code)

```
I have a Python function designed for the LabVIEW Python Node. It is called from LabVIEW
via Functions → Connectivity → Python → Python Node.

THE ERROR IS: [paste LabVIEW error number and description, e.g., "Error 1672: Python session failed"]

MY PYTHON FILE (signal_demo.py):
[PASTE YOUR ENTIRE PYTHON FILE HERE]

LABVIEW SETUP:
- Module path string: C:\LabVIEW\python\signal_demo.py
- Function name string: generate_signal
- Number of input terminals wired: [e.g., 6]
- Return type set to: [e.g., 1D DBL Array / Cluster of DBL, DBL, String / scalar DBL]
- Python version (from CMD): [e.g., Python 3.14.0]
- Bitness (from CMD): [64 or 32]

Please diagnose and fix. Apply these rules:
1. Scalars: plain float, int, str only
2. 1D arrays: plain Python list[float] — return type terminal must be empty 1D DBL Array constant
3. Tuple return for multiple mixed outputs
4. No default argument values
5. No module-level mutable state
6. numpy is allowed (import at top of file only)
7. Verify function name matches exactly — LabVIEW is case-sensitive
```

---

### 4. Fix the Array Return SystemError

```
My LabVIEW Python Node is throwing:
  "Failed when trying to convert field of tuple at index: 0
   Inner error: SystemError tupleobject.c:107: bad argument to internal function"

This means the return type terminal in LabVIEW is wired as a scalar DBL instead of a 1D Array.

PYTHON SIDE — return a plain list of float, not a tuple wrapping a list:
  return y             # CORRECT:  plain list[float]
  # return (y,)       # WRONG:    tuple containing a list — causes this exact error
  # return np.array(y) # WRONG:   numpy array — not accepted by Python Node

LABVIEW SIDE — fix the return type terminal:
  1. Right-click the return value terminal on the Python Node → Create → Constant
  2. Right-click that constant → Data Operations → Make Array
     (OR: delete it and use Functions → Programming → Array → Array Constant,
      then drop a DBL Numeric Constant inside it)
  3. Confirm the element type is DBL
  4. Wire the empty 1D DBL Array constant to the return type terminal
  5. The wire color should be dark orange/brown (array), not thin orange (scalar)

Now rewrite my function [PASTE FUNCTION HERE] so it returns list[float] directly
and add a __main__ smoke test that confirms isinstance(result, list) and all values are float.
```

---

### 5. Fix a Waveform That Looks Wrong

```
My LabVIEW waveform graph shows [describe: flat line / triangle shape / amplitude ~1e-10 / wrong frequency].

My Python function inputs are:
  amplitude     = [value]
  freq_start    = [value] Hz
  freq_stop     = [value] Hz
  dt            = [value] seconds
  waveform_type = "[Sine/Square]"
  n_samples     = [value]

Diagnose which of these common problems applies and fix the function:

1. dt too large → violates Nyquist: dt must be < 1 / (2 * max_frequency)
   For 10 Hz sine: dt must be < 0.05 s
   Recommended: dt = 0.001 (1 kHz sample rate gives 100 samples per cycle at 10 Hz)

2. n_samples too small or zero → waveform has too few points to resolve the shape
   Recommended: n_samples = 1000 for a 1-second record at dt = 0.001

3. Amplitude ~1e-10 (near zero) → amplitude control is not wired to the correct
   input terminal on the Python Node — check the block diagram terminal order

4. Phase substitution instead of phase accumulation → do NOT use sin(2*pi*f*i*dt)
   CORRECT approach: accumulate phase sample-by-sample to support sweeps:
     phase += 2 * pi * f * dt
     y[i] = amplitude * sin(phase)

Provide the corrected function and the recommended front panel values for a clean 10 Hz sine wave.
```

---

### 6. Extend with a New Signal Type

```
I have signal_demo.py for the LabVIEW Python Node. It currently supports:
sine, square, triangle, saw, noise, chirp, step

Add a new signal type called "[your_type]" that: [describe the waveform or algorithm]

Keep all existing rules:
1. No default args — all parameters always positional
2. Return list[float] — plain Python list, not a numpy array
3. No new imports except numpy (already imported at top of file)
4. Add the new type to the __main__ smoke test
5. Preserve all existing signal types exactly — do not modify them
```

---

### 7. Generate Training Data from well_model.py for DeepLTK

```
I have a Python function step_oilfield() in well_model.py used as a LabVIEW Python Node.
Write a standalone script that:

1. Imports step_oilfield from well_model
2. Runs a grid of operating points:
   - mu_cp in [0.5, 1.0, 5.0, 10.0, 50.0] cP
   - valve_pos in [0.1, 0.3, 0.5, 0.7, 1.0]
   - P_wh_init in [200, 400, 600, 800] psi
3. For each combination, runs 100 timesteps with dt_s=0.5
4. Records each step as a row: [P_wh_in, Q_in, mu, valve_pos, P_wh_out, Q_out]
5. Saves all rows to well_training_data.csv
6. Prints: total rows written, min/max P_wh, min/max Q

Use only numpy and csv (no pandas). Add a __main__ guard.
```

---

### 8. Port a MATLAB/Simulink Function to LabVIEW Python Node

```
Convert this MATLAB function to Python for use as a LabVIEW Python Node:

[PASTE MATLAB CODE HERE]

Requirements for the Python version:
1. Maintain identical numerical behaviour to MATLAB within float64 precision
2. Scalars: plain float or str (no arrays, no dicts)
3. 1D array outputs: plain list[float] — not numpy arrays
4. Multiple outputs: tuple
5. No default argument values
6. numpy is allowed for math operations (use numpy.sin, numpy.pi, etc.)
7. Include __main__ smoke test comparing to at least 3 known MATLAB output values
8. Note any MATLAB built-in functions that need numpy equivalents
```

---

### 9. Write a LabVIEW VI Description for Documentation

```
I have a LabVIEW VI that calls this Python function via the Python Node:

FUNCTION NAME: [name]
PYTHON FILE: C:\LabVIEW\python\[file].py
INPUTS: [list with LabVIEW type, Python type, and ranges]
OUTPUTS: [list with LabVIEW type, Python type]
PURPOSE: [what the VI does overall]

Write:
1. A VI Description (100 words max) suitable for LabVIEW → File → VI Properties → Documentation
2. A connector pane description for each terminal (name, direction, type, description)
3. Front panel control/indicator list with recommended LabVIEW control types
4. A one-paragraph note for the cRIO developer guide
```

---

## LabVIEW ↔ Python Complete Data Type Mapping

> Source: NI Knowledge Base kA03q000000oyaHCAQ — *Passing Python Data Structures To/From LabVIEW*

### Scalar Types

| LabVIEW Type | Python Type | Notes |
|---|---|---|
| DBL (double) | `float` | Most common numeric type |
| SGL (single) | `float` | Python always uses float64 internally |
| I32 (integer) | `int` | Cast with `int()` if needed |
| U32 (unsigned) | `int` | Same treatment as I32 |
| Boolean | `int` | Pass as `1` or `0` — no native bool |
| String | `str` | Enum and ring controls pass their string label |

### Array Types

| LabVIEW Type | Python Return Type | Return Terminal Wiring |
|---|---|---|
| 1D Array of DBL | `list[float]` | Empty **1D DBL Array** constant |
| 1D Array of I32 | `list[int]` | Empty **1D I32 Array** constant |
| 1D Array of String | `list[str]` | Empty **1D String Array** constant |
| 2D Array of DBL | `list[list[float]]` | Empty **2D DBL Array** constant |

> ⚠️ **Critical wiring rule:** The return type terminal MUST be wired with an Array constant
> matching the element type. Wiring a scalar DBL constant causes:
> `SystemError: tupleobject.c:107 bad argument to internal function`

### Cluster / Multiple Return Types

| LabVIEW Type | Python Return | Example |
|---|---|---|
| Cluster of (DBL, DBL) | `tuple[float, float]` | `return (pressure, flow)` |
| Cluster of (DBL, String) | `tuple[float, str]` | `return (value, "status_ok")` |
| Cluster of (DBL, 1D Array of DBL) | `tuple[float, list[float]]` | `return (rms, samples)` |

### What Python CANNOT Return to LabVIEW

| Type | Why It Fails | Use Instead |
|---|---|---|
| `np.ndarray` | Not a plain Python type | `list[float]` |
| `np.float64` / `np.float32` | NumPy scalar, not Python scalar | `float(value)` |
| `dict` | No LabVIEW equivalent | Cluster → `tuple` |
| `(list,)` tuple wrapping a list | Causes `tupleobject.c:107` | Return `list` directly |
| Classes / objects | Not supported | Decompose to scalars/lists |

---

## Constraint Reference Card

| Constraint | Correct ✓ | Incorrect ✗ |
|---|---|---|
| Single array return | `return [v1, v2, v3]` | `return ([v1, v2, v3],)` |
| Multiple return with array | `return (scalar, [v1, v2])` | `return np.array(...)` |
| Scalar return | `return (a, b)` tuple of floats | `return {"key": val}` dict |
| Input args | All positional, **no defaults** | `def f(x, y=0.0)` |
| Numeric types | `float`, `int`, `str` only | `np.float32`, `np.int64` |
| Boolean | `int` (0 or 1) | Python native `bool` |
| State variables | Passed in + returned out | Module-level `global state = {}` |
| Imports | Top of file only | Inside function body |
| numpy | `import numpy as np` at top | pandas, scipy, tensorflow |
| Multiple returns | Tuple → LabVIEW Cluster | Multiple `return` statements |
| Array return terminal | Empty 1D DBL Array constant | Scalar DBL constant |
| Function name | Lowercase with underscores | Mixed case must match exactly |

---

## Waveform Debugging Checklist

| Symptom | Likely Cause | Fix |
|---|---|---|
| Amplitude ~1e-10 (near zero) | Amplitude control not wired to first input terminal | Verify terminal wiring order on block diagram |
| Triangle shape instead of sine | `dt` too large — violates Nyquist | Use `dt = 0.001` for signals ≤ 100 Hz |
| Flat line | `n_samples` = 0 or 1 | Set `n_samples = 1000` minimum |
| Correct shape but wrong time scale | `dt` and `n_samples` mismatch | Record length = `n_samples × dt` seconds |
| Distorted sweep / phase jumps | Frequency substituted, not accumulated | Use `phase += 2π·f·dt` each sample |
| Error 1671 — function not found | Wrong file path or old function name | Confirm `def generate_waveform(` exists in target file |
| `SystemError tupleobject.c:107` | Return terminal wired as scalar DBL | Wire return terminal with empty **1D DBL Array** constant |
| All outputs = 0.0 (silent fail) | Error not wired through | Add error cluster and wire error out |

### Recommended Front Panel Values — Clean 10 Hz Sine

| Control | Recommended Value | Reason |
|---|---|---|
| Amplitude | 2 | Visible on ±2 scale |
| Freq Start | 10 | 10 Hz target |
| Freq Stop | 10 | Equal = fixed frequency mode |
| dt | 0.001 | 1 kHz sample rate = 100 samples/cycle |
| N Samples | 1000 | 1-second record |
| Waveform Type | Sine | |

---

## numpy Installation Guide (No Admin Rights Required)

```cmd
REM Use the EXACT python.exe that LabVIEW is configured to use
REM Check: Tools → Options → Python in LabVIEW

REM Step 1 — Install to user profile (no admin needed)
"C:\Users\[username]\AppData\Local\Python\pythoncore-3.14-64\python.exe" -m pip install numpy --user

REM Step 2 — Verify install in the correct environment
"C:\Users\[username]\AppData\Local\Python\pythoncore-3.14-64\python.exe" -c "import numpy; print(numpy.__version__)"
```

> ⚠️ The PATH warning about `f2py.exe` is **safe to ignore** — those scripts are for
> Fortran compilation only and are not needed for LabVIEW waveform work.

> ⚠️ If `Access is denied` when running pip, add `--user` flag. This installs numpy
> into your user profile instead of the system directory.

---

## Environment Check Commands

Paste into CMD to verify LabVIEW's Python environment is correctly configured:

```cmd
python --version
python -c "import struct; print(struct.calcsize('P')*8)"
python -c "import numpy; print(numpy.__version__)"
where python
pip show numpy
```

Expected results: Python 3.9–3.14, bitness = **64**, numpy version displayed.

---

## Verify Function Visibility Before Running in LabVIEW

Run in CMD to see exactly which functions LabVIEW can find in your `.py` file:

```cmd
"C:\Users\[username]\AppData\Local\Python\pythoncore-3.14-64\python.exe" -c "import importlib.util; spec=importlib.util.spec_from_file_location('m', r'C:\path\to\your_file.py'); m=importlib.util.module_from_spec(spec); spec.loader.exec_module(m); print([x for x in dir(m) if not x.startswith('_')])"
```

Your function name must appear in the printed list. If it does not:
- Check for syntax errors in the file
- Verify the `def` statement is at the top level (not inside another function or class)
- Confirm the file path string is correct and uses `r'...'` raw string format

---

## Quick Error Lookup

| LV Error | Meaning | Fix |
|---|---|---|
| 1672 | Python session failed to start | Check Tools → Options → Python path; run `python --version` in CMD |
| 1671 | Module or function not found | Verify file path (case-sensitive); confirm function name matches exactly |
| 1663 | Bitness mismatch | Both LabVIEW and Python must be **64-bit** |
| `SystemError tupleobject.c:107` | Return terminal wired as scalar; Python returned a list | Wire return terminal with empty **1D DBL Array** constant |
| `TypeError` | Wrong argument count | All terminals must be wired; no default values allowed |
| All outputs = 0.0 | Silent failure / wrong terminal order | Wire error out; verify input terminal order matches function signature exactly |
| `ImportError` | Package not in LabVIEW's Python env | Run `pip install [package] --user` using LabVIEW's exact `python.exe` |
| `Access is denied` (pip) | No admin rights | Add `--user` flag to pip install command |

---

## References

- NI Python Node docs: https://www.ni.com/en/support/documentation/supplemental/18/using-the-python-node-in-labview.html
- Configuring LabVIEW + Python: https://www.ni.com/en/support/documentation/supplemental/18/configuring-labview-to-call-python-code.html
- NI KB — Advanced Datatypes with Python Node: https://knowledge.ni.com/KnowledgeArticleDetails?id=kA03q000000oyaHCAQ
- LAVA Forums: https://lavag.org/
- NI Community: https://forums.ni.com/t5/LabVIEW/bd-p/170
