#!/usr/bin/env python3
"""
Script to transform a DIMACS-formatted graph into a CNF for k-colorability using color2sat,
then solve it with kissat, saving both the CNF and solution to specified directories.
Written by ChatGPT, prompted by Michael Helm, 11810354@student.tuwien.ac.at
"""
import argparse
import subprocess
import os
import sys


def main():
    parser = argparse.ArgumentParser(
        description="Encode graph to CNF and solve with kissat."
    )
    parser.add_argument(
        'input_graph',
        help='Path to DIMACS formatted graph file (.col)'
    )
    parser.add_argument(
        'k',
        type=int,
        help='Number of colors'
    )
    parser.add_argument(
        '--color2sat',
        default='./color2sat',
        help='Path to color2sat executable'
    )
    parser.add_argument(
        '--kissat',
        default='./kissat',
        help='Path to kissat executable'
    )
    parser.add_argument(
        '--cnf-dir',
        default='cnf',
        help='Directory to save generated CNF files'
    )
    parser.add_argument(
        '--sol-dir',
        default='sol',
        help='Directory to save solution .out files'
    )
    args = parser.parse_args()

    # Ensure output directories exist
    os.makedirs(args.cnf_dir, exist_ok=True)
    os.makedirs(args.sol_dir, exist_ok=True)

    # Derive base filenames and full paths
    base = os.path.splitext(os.path.basename(args.input_graph))[0]
    cnf_filename = f"{base}_{args.k}k.cnf"
    sol_filename = f"sol_{base}_{args.k}k.out"
    cnf_path = os.path.join(args.cnf_dir, cnf_filename)
    sol_path = os.path.join(args.sol_dir, sol_filename)

    # Generate CNF file
    print(f"Generating CNF for '{base}' with k={args.k}' into '{cnf_path}'...")
    try:
        with open(cnf_path, 'w') as cnf_f:
            result = subprocess.run(
                [args.color2sat, args.input_graph, str(args.k)],
                stdout=cnf_f,
                stderr=subprocess.PIPE,
                text=True
            )
            if result.returncode != 0:
                print(f"Error: color2sat failed (exit code {result.returncode})", file=sys.stderr)
                print(result.stderr, file=sys.stderr)
                sys.exit(result.returncode)
    except FileNotFoundError:
        print(f"Error: '{args.color2sat}' not found or not executable.", file=sys.stderr)
        sys.exit(1)

    # Run kissat solver
    print(f"Running kissat on '{cnf_path}'...")
    try:
        with open(sol_path, 'w') as sol_f:
            result = subprocess.run(
                [args.kissat, cnf_path],
                stdout=sol_f,
                stderr=subprocess.PIPE,
                text=True
            )
    except FileNotFoundError:
        print(f"Error: '{args.kissat}' not found or not executable.", file=sys.stderr)
        sys.exit(1)

    # Interpret kissat exit code
    ret = result.returncode
    if ret == 10:
        print("Result: SATISFIABLE (exit code 10)")
    elif ret == 20:
        print("Result: UNSATISFIABLE (exit code 20)")
    elif ret == 0:
        print("Result: UNKNOWN or INTERRUPTED (exit code 0)")
    else:
        print(f"Result: kissat terminated with exit code {ret}", file=sys.stderr)

    print(f"CNF saved to '{cnf_path}'")
    print(f"Solution saved to '{sol_path}'")

if __name__ == '__main__':
    main()
