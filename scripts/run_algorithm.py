from pathlib import Path
import subprocess
import re

own_diff_executable_mpi = Path(__file__).parent / "../own-diff-mpi.out"
own_diff_executable_sequential = Path(__file__).parent / "../own-diff-sequential.out"
min_edit_len_regex = re.compile(r"^min edit length (\d+)$")


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
    return parse_own_diff_output(all_output)


def run_own_diff_algorithm_sequential(file_1_path, file_2_path):
    all_output = subprocess.check_output(
        [own_diff_executable_sequential, file_1_path, file_2_path],
        text=True,
    )
    return parse_own_diff_output(all_output)


def parse_own_diff_output(output: str):
    min_edit_len = None
    for line in output.splitlines():
        m = min_edit_len_regex.fullmatch(line)
        if m:
            assert min_edit_len is None
            min_edit_len = int(m[1])
    return min_edit_len