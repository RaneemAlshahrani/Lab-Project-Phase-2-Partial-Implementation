#!/usr/bin/env python3
"""Run the documented demos and save original stdout plus runtime artifacts.

Only these generated evidence subfolders are recreated; runtime data/ is untouched.
"""
from pathlib import Path
import json
import shutil
import subprocess

root = Path(__file__).resolve().parents[1]
evidence = root / 'evidence'
cases = [('reference', 'reference.txt'), ('resources', 'resources.txt'),
         ('notes', 'notes_first.txt'), ('five_tasks', 'five_tasks.txt')]
for name, script in cases:
    directory = evidence / name
    if directory.exists():
        shutil.rmtree(directory)
    (directory / 'notes').mkdir(parents=True)
    result = subprocess.run([str(root / 'minios'), '--batch', '--data-dir', str(directory)],
                            input=(root / 'tests' / script).read_text(), text=True,
                            capture_output=True, check=True)
    assert not result.stderr, result.stderr
    (evidence / (name + '_console.txt')).write_text(result.stdout)
    if name == 'notes':
        result = subprocess.run([str(root / 'minios'), '--batch', '--data-dir', str(directory)],
                                input=(root / 'tests/notes_restart.txt').read_text(), text=True,
                                capture_output=True, check=True)
        assert not result.stderr, result.stderr
        (evidence / 'notes_restart_console.txt').write_text(result.stdout)

# The test-result files retain the complete assertions and observed outcomes.
result = subprocess.run(['python3', 'tests/run_tests.py'], cwd=root,
                        text=True, capture_output=True, check=True)
assert not result.stderr, result.stderr
(evidence / 'test_console.txt').write_text(result.stdout)
provenance = {
    'application': 'C11 MiniOS for a Study Room',
    'commands': ['./minios --batch --data-dir evidence/SCENARIO < tests/SCENARIO.txt',
                 'python3 tests/run_tests.py'],
    'note': 'Console text is original captured stdout; image excerpts are rendered from these files.'
}
(evidence / 'capture_provenance.json').write_text(json.dumps(provenance, indent=2) + '\n')
print('Evidence collected: reference, resources, notes + restart, five tasks, T1-T12.')
