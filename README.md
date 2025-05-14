# Graph k-Colorability → SAT Converter

This project provides tools to encode the graph k-colorability problem as a SAT instance in DIMACS CNF format and (optionally) solve it with the **kissat** SAT solver. It consists of:

- **`color2sat.c`**: A C program that reads a graph in DIMACS `.col` format plus an integer *k*, and emits the equivalent CNF formula to stdout.
- **`combined_script.py`**: A Python wrapper that calls `color2sat`, runs **kissat**, and manages output directories.

---

## Prerequisites

- **Compiler & Build Tools**  
  - GCC (with C11 support)  
  - GNU Make  
- **Python**  
  - Python 3.6+ (no external packages required)  
- **SAT Solver**  
  - [kissat](https://github.com/arminbiere/kissat) (v2.1.0 or newer recommended)  
- **Graph Instances**  
  - Your input graphs must be in DIMACS `.col` format.

---

## Building the Encoder

1. Clone or unpack this repository.  
2. From the project root, run:

   ```bash
   make

This builds the `color2sat` (and `k-colorability`) executables using the provided `Makefile`.

3. **Alternatively**, compile by hand:

   ```bash
   gcc -std=c11 -O3 -DNDEBUG -march=native -flto \
       -D_DEFAULT_SOURCE -D_BSD_SOURCE -D_SVID_SOURCE \
       -D_POSIX_C_SOURCE=200809L \
       -o color2sat color2sat.c
   ```

---

## Usage

### 1. Directly with `color2sat`

Convert a `.col` graph to CNF:

```bash
./color2sat <input_graph>.col <k> > <output>.cnf
```

* `<input_graph>.col`: Path to your DIMACS graph file (use `-` to read from stdin).
* `<k>`: Number of colors (positive integer).
* Redirect to a `.cnf` file or pipe into any SAT solver.

**Example**:

```bash
./color2sat graphinstances/le450_15a.col 15 > cnf/le450_15a_15k.cnf
```

### 2. Using the Python Wrapper

The `combined_script.py` automates encoding, solving, and saving:

```bash
python3 combined_script.py \
  [--color2sat ./color2sat] \
  [--kissat    ./kissat] \
  [--cnf-dir   cnf] \
  [--sol-dir   sol] \
  <input_graph>.col <k>
```

* `--color2sat`: Path to the `color2sat` executable (default: `./color2sat`).
* `--kissat`:    Path to the `kissat` executable (default: `./kissat`).
* `--cnf-dir`:   Directory to store generated CNFs (default: `cnf`).
* `--sol-dir`:   Directory to store solver outputs (default: `sol`).

#### Example Run

```bash
$ python3 combined_script.py graphinstances/le450_15a.col 15
Generating CNF for 'le450_15a' with k=15' into 'cnf/le450_15a_15k.cnf'...
Running kissat on 'cnf/le450_15a_15k.cnf'...
Result: SATISFIABLE (exit code 10)
CNF saved to 'cnf/le450_15a_15k.cnf'
Solution saved to 'sol/sol_le450_15a_15k.out'
```

---

## Project Structure

```
.
├── Makefile
├── color2sat.c
├── combined_script.py
├── cnf/           ← Generated CNF files
├── sol/           ← Generated solution files
└── graphinstances/
    └── *.col      ← Example DIMACS graphs
```

---


