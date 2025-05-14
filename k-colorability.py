#!/usr/bin/env python3
"""
k-colorability: Python wrapper for color2sat + MiniSAT
Usage:
    k-colorability.py [-k K] [--minisat-op "opts"] [-o outfile] [inputfile]

If -k is omitted, tries K=2..N until SAT.  Reads DIMACS .col graph (file or stdin),
runs `color2sat - k` to generate CNF, pipes into MiniSAT with given options,
and outputs the first satisfying K and the model.
"""
import sys
import argparse
import subprocess
import tempfile
import os
import shutil
import re

def load_graph(path):
    if path == '-':
        return sys.stdin.read()
    with open(path, 'r') as f:
        return f.read()


def parse_max_k(graph_text):
    for line in graph_text.splitlines():
        if line.startswith('p'):
            parts = line.split()
            if len(parts) >= 4 and parts[1] == 'edge':
                try:
                    return int(parts[2])
                except ValueError:
                    pass
    sys.exit('ERROR: invalid or missing "p edge N M" header')


def run_for_k(graph_text, k, minisat_opts):
    # 1) color2sat
    p1 = subprocess.Popen(
        ['./color2sat', '-', str(k)],
        stdin=subprocess.PIPE,
        stdout=subprocess.PIPE,
        stderr=sys.stderr,
        text=True
    )
    cnf_text, err1 = p1.communicate(graph_text)
    if p1.returncode != 0:
        print(f'ERROR: color2sat failed for k={k}', file=sys.stderr)
        print(err1, file=sys.stderr)
        return False, None

    # 2) run minisat with temp result file
    with tempfile.NamedTemporaryFile(delete=False) as tf:
        resfile = tf.name
    cmd = ['./minisat'] + minisat_opts + [resfile]
    print(f'Running command: {" ".join(cmd)}', file=sys.stderr)
    p2 = subprocess.Popen(
        cmd,
        stdin=subprocess.PIPE,
        stdout=subprocess.PIPE,
        stderr=sys.stderr,
        text=True
    )
    out2, err2 = p2.communicate(cnf_text)
    if p2.returncode not in (10, 0):  # 10=SAT, 20=UNSAT for vanilla minisat
        # treat non-zero/non-10 as failure
        return False, None
    # read result file
    if not os.path.exists(resfile):
        return False, None
    with open(resfile, 'r') as rf:
        lines = rf.read().splitlines()
    #os.remove(resfile)
    if lines and lines[0].strip() == 'SAT':
        return True, lines
    return False, None


def main():
    ap = argparse.ArgumentParser(add_help=False)
    ap.add_argument('-k', type=int, help='target k (optional)')
    ap.add_argument('--minisat-op', dest='minisat_op', default='',
                    help='options for minisat, e.g. "-pre=once -verbosity=1"')
    ap.add_argument('-o', dest='outfile', help='write output to file')
    ap.add_argument('inputfile', nargs='?', default='-')
    ap.add_argument('-h', '--help', action='help', help='show this help message and exit')
    args = ap.parse_args()

    graph_text = load_graph(args.inputfile)
    max_k = parse_max_k(graph_text)
    start_k = args.k if args.k and args.k > 1 else 2
    minisat_opts = args.minisat_op.split() if args.minisat_op else []

    found = False
    model = None
    found_k = None
    for k in range(start_k, (args.k or max_k) + 1):
        sat, lines = run_for_k(graph_text, k, minisat_opts)
        if sat:
            found = True
            found_k = k
            model = lines
            break

    out_lines = []
    if found:
        out_lines.append(f'k = {found_k}')
        out_lines.extend(model)
    else:
        out_lines.append('UNSAT for all k up to ' + str((args.k or max_k)))

    if args.outfile:
        with open(args.outfile, 'w') as of:
            of.write("\n".join(out_lines) + "\n")
    else:
        print("\n".join(out_lines))
    sys.exit(0 if found else 1)

if __name__ == '__main__':
    main()
