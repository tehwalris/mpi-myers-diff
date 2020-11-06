from .diff_sizes import update_test_case_diff, get_test_case_dirs
from .gen_random_input import generate_and_save_test_case
from pathlib import Path
import subprocess
import multiprocessing
import re
from termcolor import colored
import numpy as np
import argparse

own_diff_executable = Path(__file__).parent / "../a.out"
min_edit_len_regex = re.compile(r"^min edit length (\d+)$")

parser = argparse.ArgumentParser(
    description="Test our diff algorithm with random test cases"
)
parser.add_argument(
    "--num-tests", type=int, default=10, help="number of randomized tests to run"
)
parser.add_argument(
    "--early-stop",
    default=False,
    action="store_true",
    help="skip running further tests as soon as one fails",
)
parser.add_argument(
    "--mpi-procs",
    type=int,
    default=multiprocessing.cpu_count(),
    help="number of processes to run our distributed algorithm with",
)


def run_own_diff_algorithm(file_1_path, file_2_path, mpi_processes):
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


def gen_random_generation_config():
    config = {
        "strategy": np.random.choice(
            ["independent", "add", "remove", "addremove"], p=[0.1, 0.2, 0.2, 0.5]
        ),
        "length_1": np.random.randint(1, 10 ** np.random.randint(1, 3)),
        "change_strength": np.random.rand()
        * np.random.choice([0.3, 1], p=[0.75, 0.25]),
        "chunkiness": np.random.rand(),
        "distribution": np.random.choice(["uniform", "zipf"], p=[0.25, 0.75]),
    }
    if config["strategy"] == "independent":
        config["change_strength"] = 1
        config["chunkiness"] = 0
    return config


if __name__ == "__main__":
    args = parser.parse_args()

    some_tests_failed = False
    for i in range(args.num_tests):
        print(i, end=" ", flush=True)

        config = gen_random_generation_config()
        test_case_dir = generate_and_save_test_case(
            config, temporary=True, detailed_name=False
        )
        print(test_case_dir.name, config, flush=True)

        print("Running golden implementation", flush=True)
        golden_diff_size = update_test_case_diff(test_case_dir)
        print("Running own implementation", flush=True)
        own_diff_size = run_own_diff_algorithm(
            test_case_dir / "in_1.txt",
            test_case_dir / "in_2.txt",
            args.mpi_procs,
        )

        if own_diff_size == golden_diff_size:
            print(colored("PASS", "green"))
        else:
            print(
                f'{colored("FAIL", "red")} want {golden_diff_size} got {own_diff_size}'
            )
            some_tests_failed = True
            if args.early_stop:
                break

    if some_tests_failed:
        exit(1)
