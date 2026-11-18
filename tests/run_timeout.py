#!/usr/bin/env python3
"""tests/run_timeout.py SECONDS CMD... — run CMD with a time limit (portable stand-in for GNU timeout,
which macOS lacks). Exit status: CMD's own, or 124 if it was killed for running too long."""
import subprocess
import sys

try:
    sys.exit(subprocess.run(sys.argv[2:], timeout=float(sys.argv[1])).returncode)
except subprocess.TimeoutExpired:
    sys.exit(124)
