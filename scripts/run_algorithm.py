from pathlib import Path
import subprocess
import re
from typing import List

own_diff_executable_mpi = Path(__file__).parent / "../own-diff-mpi.out"
own_diff_executable_sequential = Path(__file__).parent / "../own-diff-sequential.out"
diffutils_executable = Path(__file__).parent / "../diffutils.out"


def run_own_diff_algorithm_mpi(file_1_path, file_2_path, mpi_processes):
    all_output = subprocess.check_output(
        [
            "mpiexec",
            "-np",
            str(mpi_processes),
            own_diff_executable_mpi,
            file_1_path,
            file_2_path,
        ],
        text=True,
    )
    return OwnDiffOutput(all_output)


def run_own_diff_algorithm_sequential(file_1_path, file_2_path):
    all_output = subprocess.check_output(
        [own_diff_executable_sequential, file_1_path, file_2_path],
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
        "key": "micros_until_len",
        "regex": re.compile(r"^Solution \[μs\]:\s*(\d+)$"),
        "extract": lambda m: int(m[1]),
    },
]


class OwnDiffOutput(RegexExtractedOutput):
    def __init__(self, output: str):
        super().__init__(output, own_diff_output_fields)


diffutils_output_fields = [
    {
        "key": "micros_until_len",
        "regex": re.compile(r"^Time \[µs\]:\s*(\d+)$"),
        "extract": lambda m: int(m[1]),
    },
]


class DiffutilsOutput(RegexExtractedOutput):
    def __init__(self, output: str):
        super().__init__(output, diffutils_output_fields)