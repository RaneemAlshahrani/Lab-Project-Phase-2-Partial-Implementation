#!/usr/bin/env python3
"""Reproducible integration tests. Python is test tooling; all application code is C11."""
import argparse
import csv
import json
import os
from pathlib import Path
import re
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
PARSER = argparse.ArgumentParser()
PARSER.add_argument('--sanitize', action='store_true')
ARGS = PARSER.parse_args()
BIN = ROOT / ('minios_asan' if ARGS.sanitize else 'minios')
RESULTS = []


def require(condition, detail):
    if not condition:
        raise AssertionError(detail)


def run(directory, commands):
    p = subprocess.run([str(BIN), '--batch', '--data-dir', str(directory)],
                       input=commands, text=True, capture_output=True, timeout=15)
    require(p.returncode == 0, f'Process failed ({p.returncode}): {p.stderr}\n{p.stdout}')
    require(not p.stderr, f'Unexpected diagnostics: {p.stderr}')
    require('FATAL' not in p.stdout, 'Invariant failure')
    require(p.stdout.endswith('Shutdown: used_bytes=0 allocations=0\n'), 'Cleanup summary missing')
    return p.stdout


def case(test_id, title, expected, action):
    try:
        observed = action()
        RESULTS.append(dict(id=test_id, title=title, expected=expected, observed=observed, status='PASS'))
        print(f'{test_id} PASS | {title}')
    except Exception as exc:
        RESULTS.append(dict(id=test_id, title=title, expected=expected, observed=str(exc), status='FAIL'))
        print(f'{test_id} FAIL | {title}: {exc}')


def in_directory(action):
    with tempfile.TemporaryDirectory(prefix='minios-test-') as temporary:
        d = Path(temporary)
        (d / 'notes').mkdir()
        return action(d)


def reference(d):
    out = run(d, (ROOT / 'tests/reference.txt').read_text())
    rows = list(csv.DictReader((d / 'trace.csv').open()))
    require([(int(r['pid']), int(r['start']), int(r['end'])) for r in rows] ==
            [(1, 0, 2), (2, 2, 4), (3, 4, 6), (1, 6, 8), (2, 8, 9)], 'Dispatch mismatch')
    metrics = list(csv.DictReader((d / 'metrics.csv').open()))
    require([(r['completion'], r['waiting'], r['response']) for r in metrics] ==
            [('8', '4', '0'), ('9', '6', '2'), ('6', '4', '4')], 'Metric mismatch')
    require('turnaround=7.67 waiting=4.67 response=2.00' in out, 'Mean mismatch')
    return 'Order P1,P2,P3,P1,P2; ends 2,4,6,8,9; means 7.67/4.67/2.00.'


def persistence(d):
    run(d, 'note create revision CPU scheduling notes\nnote append revision Round robin uses a FIFO queue.\nquit\n')
    expected = 'CPU scheduling notes\nRound robin uses a FIFO queue.\n'
    require((d / 'notes/revision.txt').read_bytes() == expected.encode(), 'Written bytes mismatch')
    out = run(d, 'note read revision\nnote create revision Replacement\nquit\n')
    require(expected in out and out.count('ERROR:') == 1, 'Restart/overwrite check failed')
    require((d / 'notes/revision.txt').read_bytes() == expected.encode(), 'Existing note overwritten')
    out = run(d, 'note delete revision\nquit\n')
    require('OK: note delete' in out and not (d / 'notes/revision.txt').exists(), 'Delete failed')
    return 'Exact bytes preserved across process restart; duplicate create rejected; deletion verified.'


def blocking(d):
    out = run(d, 'task create A 4 64\ntask create B 2 64\nblock 1\nstep\ntasks\nwake 1\nwake 1\nrun\ncheck\nquit\n')
    rows = list(csv.DictReader((d / 'trace.csv').open()))
    require([r['pid'] for r in rows] == ['2', '1', '1'], 'Blocked task dispatched or duplicate wake')
    require(out.count('ERROR:') == 1 and 'WAITING' in out and 'CHECK PASS' in out, 'State check failed')
    return 'P2 executes while P1 is WAITING; wake enqueues P1 once; duplicate wake rejected.'


def edge(d):
    out = run(d, 'run\nquantum 10\ntask create Short 1 1\nrun\ncheck\nquit\n')
    rows = list(csv.DictReader((d / 'trace.csv').open()))
    require(len(rows) == 1 and rows[0]['end'] == '1', 'Short slice advanced by full quantum')
    require('IDLE' in out and 'CHECK PASS' in out, 'Empty/single-task check failed')
    return 'Empty run stays at tick 0; burst 1 with quantum 10 completes at tick 1.'


def capacity(d):
    commands = ''.join(f'task create Task{i} 1 1\n' for i in range(21))
    out = run(d, commands + 'check\nmemory\nquit\n')
    require(out.count('Created P') == 20 and out.count('ERROR:') == 1, 'Capacity mismatch')
    require('used=20/65536' in out and 'CHECK PASS' in out, 'Capacity changed memory unexpectedly')
    return '20 records accepted; record 21 rejected; 20 payload bytes tracked before shutdown.'


def budget(d):
    out = run(d, 'task create Full 1 65536\ntask create Excess 1 1\nmemory\nrun\nmemory\ncheck\nquit\n')
    require('used=65536/65536' in out and 'used=0/65536' in out and out.count('ERROR:') == 1,
            'Budget/accounting mismatch')
    return '65,536-byte allocation accepted; extra byte rejected; completion returns usage to zero.'


def invalid(d):
    commands = ['task create Bad abc 1', 'task create Bad -1 1', 'quantum 0',
                'task create ' + 'A' * 49 + ' 1 1', 'task create Bad 1 0',
                'task create Bad 1000001 1', 'task create Bad 1 65537',
                'task create Bad 99999999999999999999999999999999 1',
                'task create Bad 1 1 extra', 'quantum 2 extra',
                'X' * 2050 + 'task create Tail 1 1',
                'task create Good 1 1', 'tasks', 'check', 'quit']
    out = run(d, '\n'.join(commands) + '\n')
    require(out.count('ERROR:') == 11 and out.count('Created P') == 1, 'Input rejection mismatch')
    require('Created P1 Good' in out and 'CHECK PASS' in out, 'Partial creation or tail executed')
    return '11 invalid/oversized commands rejected; next valid task gets P1; oversized tail discarded.'


def file_errors(d):
    out = run(d, 'note read missing\nnote append missing text\nnote read ../outside\ncheck\nquit\n')
    require(out.count('ERROR:') == 3 and 'CHECK PASS' in out, 'File error recovery failed')
    (d / 'notes').rename(d / 'notes_saved')
    out = run(d, 'note create fail Cannot open directory\ncheck\nquit\n')
    require(out.count('ERROR:') == 1 and 'OK: note create' not in out, 'Open failure falsely succeeded')
    # Linux-specific test: /dev/full accepts open but rejects buffered write/close.
    if Path('/dev/full').exists() and hasattr(os, 'symlink'):
        (d / 'notes_saved').rename(d / 'notes')
        os.symlink('/dev/full', d / 'notes/writefail.txt')
        out = run(d, 'note append writefail Payload\ncheck\nquit\n')
        require('Note write/close failed' in out and 'OK: note append' not in out, 'Write failure hidden')
        return 'Missing/read/path/open errors recover; /dev/full write/close failure is reported.'
    return 'Missing/read/path/open errors recover; /dev/full write-failure check unavailable on this host.'


def transitions(d):
    out = run(d, 'task create A 1 1\nwake 1\nblock 999\nwake 999\nrun\nblock 1\nwake 1\ncheck\nquit\n')
    require(out.count('ERROR:') == 5 and 'CHECK PASS' in out, 'Invalid transition check failed')
    return 'Unknown PIDs, wake READY, and block/wake FINISHED rejected; invariants hold.'


def allocation_failure(d):
    out = run(d, 'task create Existing 1 10\ndebug fail-next-allocation\ntask create Fails 1 20\n'
                 'memory\ntask create Next 1 20\nrun\ncheck\nquit\n')
    require('Forced allocation failure' in out and 'used=10/65536' in out and 'Created P2 Next' in out,
            'Failed allocation consumed state')
    require('CHECK PASS' in out, 'Allocation failure broke invariants')
    return 'Forced NULL leaves existing 10 bytes unchanged; next task gets P2 and runs normally.'


def cleanup(d):
    out = run(d, 'task create A 1 100\nrun\ntask create B 5 200\nblock 2\nreset\nmemory\n'
                 'task create C 5 300\ntask create D 5 400\nblock 2\ncheck\nquit\n')
    require('used=0/65536' in out and 'CHECK PASS' in out, 'Reset cleanup mismatch')
    log = (d / 'events.log').read_text()
    require('SHUTDOWN pid=0 used_bytes=0 allocations=0' in log, 'Shutdown log mismatch')
    return 'Completion, reset with WAITING task, and exit with READY/WAITING tasks release all payloads.'


def sessions(d):
    out = run(d, 'session start Focus 5 64\nstep\nstep\nstep\nstep\nrun\ncheck\nquit\n')
    require(out.count('SESSION COMPLETE: P1 Focus at tick 5') == 1, 'Session notification count/tick mismatch')
    require((d / 'events.log').read_text().count('event=SESSION_COMPLETE') == 1, 'Duplicate event')
    return 'Three slices (2+2+1) complete at tick 5; console and event log each contain one notice.'


CASES = [
    ('T1', 'Reference scheduling and metrics', 'Exact proposal trace and metrics.', reference),
    ('T2', 'Persistent note lifecycle', 'Saved bytes survive restart; append and delete succeed.', persistence),
    ('T3', 'Block/wake scheduling', 'WAITING excluded; wake appends exactly once.', blocking),
    ('T4', 'Empty, single, and short burst', 'Safe idle; slice=min(quantum, remaining).', edge),
    ('T5', '20-task capacity', 'First 20 accepted; 21st rejected without corruption.', capacity),
    ('T6', 'Exact memory budget', '65,536 accepted; extra byte rejected; memory reclaimed.', budget),
    ('T7', 'Input validation', 'Invalid numeric, name, oversized commands leave no partial task.', invalid),
    ('T8', 'File failure recovery', 'Read/open/write failures reported; CLI continues.', file_errors),
    ('T9', 'Invalid transitions and PIDs', 'Invalid transitions rejected without queue changes.', transitions),
    ('T10', 'Forced allocation failure', 'No PID, task, queue, or memory counter committed.', allocation_failure),
    ('T11', 'Memory lifecycle cleanup', 'Zero tracked payload bytes after completion/reset/exit.', cleanup),
    ('T12', 'Study session completion', 'Exactly one notice on actual completion.', sessions),
]

for identifier, title, expected, action in CASES:
    case(identifier, title, expected, lambda action=action: in_directory(action))

core = ROOT / 'tests' / ('test_core_asan' if ARGS.sanitize else 'test_core')
p = subprocess.run([str(core)], text=True, capture_output=True, timeout=15)
core_ok = p.returncode == 0 and not p.stderr and 'CORE ASSERTIONS PASS' in p.stdout
print(p.stdout.strip() if core_ok else f'CORE ASSERTIONS FAIL: {p.stderr}')

evidence = ROOT / 'evidence'
evidence.mkdir(exist_ok=True)
suffix = '_sanitizer' if ARGS.sanitize else ''
(evidence / f'test_results{suffix}.json').write_text(json.dumps(
    {'suite': ('ASan+UBSan; ASAN_OPTIONS=' + os.environ.get('ASAN_OPTIONS', '(default)'))
              if ARGS.sanitize else 'GCC standard build',
     'cases': RESULTS, 'core_assertions': 'PASS' if core_ok else 'FAIL',
     'core_output': p.stdout, 'core_diagnostics': p.stderr}, indent=2) + '\n')
with (evidence / f'test_results{suffix}.csv').open('w', newline='') as f:
    writer = csv.DictWriter(f, fieldnames=['id', 'title', 'expected', 'observed', 'status'])
    writer.writeheader()
    writer.writerows(RESULTS)
passed = sum(r['status'] == 'PASS' for r in RESULTS)
print(f'Result: {passed}/12 integration cases; core assertions {"PASS" if core_ok else "FAIL"}.')
raise SystemExit(0 if passed == 12 and core_ok else 1)
