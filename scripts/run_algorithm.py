from pathlib import Path
import subprocess
import re
from typing import List

# Executables:
own_diff_executable_mpi_master = Path(__file__).parent / "../bin/own-diff-mpi-main.out"
own_diff_executable_mpi_no_master = (
    Path(__file__).parent / "../bin/own-diff-mpi-no-master.out"
)
own_diff_executable_mpi_no_sync_layers = (
    Path(__file__).parent / "../bin/own-diff-mpi-no-sync-layers.out"
)
own_diff_executable_sequential = (
    Path(__file__).parent / "../bin/own-diff-sequential.out"
)
own_diff_executable_sequential_fast_snakes = (
    Path(__file__).parent / "../bin/own-diff-sequential-fast-snakes.out"
)
diffutils_executable = Path(__file__).parent / "../bin/diffutils.out"


def run_diff_algorithm_mpi(
    file_1_path, file_2_path, mpi_processes, mpi_executable_path, edit_script_path=None
):
    # TODO pascal: KeyboardInterrupt doesn't get passed down and the mpi keeps running in the background hogging resources.
    # TODO set timeout?

    command = [
        "mpiexec",
        "-np",
        str(mpi_processes),
        mpi_executable_path,
        file_1_path,
        file_2_path,
    ]
    if edit_script_path is not None:
        command.append(edit_script_path)

    all_output = subprocess.check_output(command, text=True)
    return OwnDiffOutput(all_output)


def run_own_diff_algorithm_sequential(file_1_path, file_2_path, executable_path):
    all_output = subprocess.check_output(
        [executable_path, file_1_path, file_2_path],
        text=True,
    )
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