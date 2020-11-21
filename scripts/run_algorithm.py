from pathlib import Path
import subprocess
import re

own_diff_mpi_executable = Path(__file__).parent / "../own-diff-mpi.out"
min_edit_len_regex = re.compile(r"^min edit length (\d+)$")


def run_own_diff_algorithm_mpi(file_1_path, file_2_path, mpi_processes):
    all_output = subprocess.check_output(
        [
            "mpiexec",
            "-np",
            str(mpi_processes),
            own_diff_mpi_executable,
            file_1_path,
            file_2_path,
        ],
        text=True,
    )
    min_edit_len = None
    for line in all_output.splitlines():
        m = min_edit_len_regex.fullmatch(line)
        if m:
            assert min_edit_len is None
            min_edit_len = int(m[1])
    return min_edit_len