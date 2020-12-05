from pathlib import Path
import subprocess
import re
from typing import List
import os
import signal
import time

# Executables:
own_diff_executable_mpi_master = Path(__file__).parent / "../bin/own-diff-mpi-main.out"
own_diff_executable_mpi_no_master = Path(__file__).parent / "../bin/own-diff-mpi-no-master.out"
own_diff_executable_sequential = Path(__file__).parent / "../bin/own-diff-sequential.out"
own_diff_executable_sequential_fast_snakes = Path(__file__).parent / "../bin/own-diff-sequential-fast-snakes.out"
diffutils_executable = Path(__file__).parent / "../bin/diffutils.out"

# https://stackoverflow.com/questions/32222681/how-to-kill-a-process-group-using-python-subprocess
def kill_process(pid):
    os.killpg(os.getpgid(pid), signal.SIGTERM)


def run_diff_algorithm_mpi(file_1_path, file_2_path, mpi_processes, mpi_executable_path, edit_script_path=None):
    # TODO pascal is timeout of 6min OK?

    args = ["mpiexec",
            "-np",
            str(mpi_processes),
            mpi_executable_path,
            file_1_path,
            file_2_path]

    if edit_script_path is not None:
        args.append(edit_script_path)

    with subprocess.Popen(args, text=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE, start_new_session=True) as proc:
        try:
            all_output, error = proc.communicate(timeout=360)

        except Exception as e: 
            # kill child and pass exception on to handle/log in benchmark
            # this means that KeyboardInterrupt properly kills both python AND the MPI processes
            proc.terminate()
            kill_process(proc.pid) # kill children via process group
            time.sleep(5)
            proc.kill()
            proc.communicate()     # clear buffers
            raise 

    return OwnDiffOutput(all_output)


def run_own_diff_algorithm_sequential(file_1_path, file_2_path, executable_path):
    
    args = [executable_path, file_1_path, file_2_path]

    with subprocess.Popen(args, text=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE) as proc:
        try:
            all_output, error = proc.communicate()

        except Exception as e: 
            # kill child and pass exception on to handle/log in benchmark
            # this means that KeyboardInterrupt properly kills both python AND the processes
            proc.terminate()
            proc.kill()
            proc.communicate()
            raise 

    return OwnDiffOutput(all_output)


def run_diffutils(file_1_path, file_2_path):
    result = subprocess.run(
        [diffutils_executable, "--minimal", file_1_path, file_2_path],
        capture_output=True,
        text=True,
    )
    if result.returncode not in [0, 1]:
        raise RuntimeError(f"diff command returned {result.returncode}")
    return DiffutilsOutput(result.stdout)


class RegexExtractedOutput:
    def __init__(self, output: str, field_configs: List):
        found_fields = set()
        for line in output.splitlines():
            for field in field_configs:
                m = field["regex"].fullmatch(line)
                if m:
                    assert field["key"] not in found_fields
                    setattr(self, field["key"], field["extract"](m))
                    found_fields.add(field["key"])

        missing_fields = {field["key"] for field in field_configs} - found_fields
        if missing_fields:
            raise ValueError(
                f"Fields not found in output: {', '.join(sorted(missing_fields))}"
            )


own_diff_output_fields = [
    {
        "key": "min_edit_len",
        "regex": re.compile(r"^min edit length (\d+)$"),
        "extract": lambda m: int(m[1]),
    },
    {
        "key": "micros_input",
        "regex": re.compile(r"^Read Input \[μs\]:\s*(\d+)$"),
        "extract": lambda m: int(m[1]),
    },
    {
        "key": "micros_precompute",
        "regex": re.compile(r"^Precompute \[μs\]:\s*(\d+)$"),
        "extract": lambda m: int(m[1]),
    },
    {
        "key": "micros_until_len",
        "regex": re.compile(r"^Solution \[μs\]:\s*(\d+)$"),
        "extract": lambda m: int(m[1]),
    },
    {
        "key": "micros_edit_script",
        "regex": re.compile(r"^Edit Script \[μs\]:\s*(\d+)$"),
        "extract": lambda m: int(m[1]),
    },
]


class OwnDiffOutput(RegexExtractedOutput):
    def __init__(self, output: str):
        super().__init__(output, own_diff_output_fields)


diffutils_output_fields = [
    {
        "key": "micros_input",
        "regex": re.compile(r"^Read Input \[μs\]:\s*(\d+)$"),
        "extract": lambda m: int(m[1]),
    },
    {
        "key": "micros_precompute",
        "regex": re.compile(r"^Precompute \[μs\]:\s*(\d+)$"),
        "extract": lambda m: int(m[1]),
    },
    {
        "key": "micros_until_len",
        "regex": re.compile(r"^Solution \[μs\]:\s*(\d+)$"),
        "extract": lambda m: int(m[1]),
    },
    {
        "key": "micros_edit_script",
        "regex": re.compile(r"^Edit Script \[μs\]:\s*(\d+)$"),
        "extract": lambda m: int(m[1]),
    },
]


class DiffutilsOutput(RegexExtractedOutput):
    def __init__(self, output: str):
        super().__init__(output, diffutils_output_fields)