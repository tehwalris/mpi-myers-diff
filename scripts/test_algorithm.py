from .diff_sizes import update_test_case_diff, get_test_case_dirs
from .gen_random_input import generate_and_save_test_case
from pathlib import Path
import subprocess
import multiprocessing
import re
from termcolor import colored

own_diff_executable = Path(__file__).parent / "../a.out"
mpi_processes = multiprocessing.cpu_count()
min_edit_len_regex = re.compile(r"^min edit length (\d+)$")


def run_own_diff_algorithm(file_1_path, file_2_path):
    all_output = subprocess.check_output(
        [
            "mpiexec",
            "-np",
            str(mpi_processes),
            own_diff_executable,
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


if __name__ == "__main__":
    for i in range(3):
        print(i, end=" ", flush=True)

        config = {
            "strategy": "addremove",
            "length_1": 50,
            "change_strength": 0.2,
            "chunkiness": 0.5,
            "distribution": "zipf",
        }
        test_case_dir = generate_and_save_test_case(
            config, temporary=True, detailed_name=False
        )
        print(test_case_dir.name, end=" ", flush=True)

        golden_diff_size = update_test_case_diff(test_case_dir)
        own_diff_size = run_own_diff_algorithm(
            test_case_dir / "in_1.txt", test_case_dir / "in_2.txt"
        )

        if own_diff_size == golden_diff_size:
            print(colored("PASS", "green"))
        else:
            print(
                f'{colored("FAIL", "red")} want {golden_diff_size} got {own_diff_size}'
            )
            print(config)
            exit(1)
