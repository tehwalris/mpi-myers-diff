from pathlib import Path
import subprocess
import re

own_diff_executable_mpi = Path(__file__).parent / "../own-diff-mpi.out"
own_diff_executable_sequential = Path(__file__).parent / "../own-diff-sequential.out"


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


own_diff_output_fields = [
    {
        "key": "min_edit_len",
        "regex": re.compile(r"^min edit length (\d+)$"),
        "extract": lambda m: int(m[1]),
    }
]


class OwnDiffOutput:
    def __init__(self, output: str):
        found_fields = set()
        for line in output.splitlines():
            for field in own_diff_output_fields:
                m = field["regex"].fullmatch(line)
                if m:
                    assert field["key"] not in found_fields
                    setattr(self, field["key"], field["extract"](m))
                    found_fields.add(field["key"])
        assert len(found_fields) == len(own_diff_output_fields)
