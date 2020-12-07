from .diff_sizes import update_test_case_diff
from .gen_random_input import generate_and_save_test_case
from . import run_algorithm
import multiprocessing
from termcolor import colored
import numpy as np
import argparse

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


def gen_random_generation_config():
    config = {
        "strategy": np.random.choice(
            ["independent", "add", "remove", "addremove"], p=[0.1, 0.2, 0.2, 0.5]
        ),
        "length_1": np.random.randint(1, 10 ** np.random.randint(2, 5)),
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
        print("Running own MPI implementation", flush=True, end="")
        own_diff_output_mpi = run_algorithm.run_diff_algorithm_mpi(
            test_case_dir / "in_1.txt",
            test_case_dir / "in_2.txt",
            args.mpi_procs,
            run_algorithm.own_diff_executable_mpi_priority,
        )
        print(f" {own_diff_output_mpi.micros_until_len:>15} μs", flush=True)
        print("Running own sequential implementation", flush=True, end="")
        own_diff_output_sequential = run_algorithm.run_own_diff_algorithm_sequential(
            test_case_dir / "in_1.txt",
            test_case_dir / "in_2.txt",
            run_algorithm.own_diff_executable_sequential,
        )
        print(f" {own_diff_output_sequential.micros_until_len:>8} μs", flush=True)
        print(
            f"Speed-up: {own_diff_output_sequential.micros_until_len/own_diff_output_mpi.micros_until_len:.2f}x"
        )

        if (
            own_diff_output_mpi.min_edit_len == golden_diff_size
            and own_diff_output_sequential.min_edit_len == golden_diff_size
        ):
            print(colored("PASS\n", "green"))
        else:
            print(
                f'{colored("FAIL", "red")} want {golden_diff_size} got {own_diff_output_mpi.min_edit_len} (MPI) and {own_diff_output_sequential.min_edit_len} (sequential) \n'
            )
            some_tests_failed = True
            if args.early_stop:
                break

    if some_tests_failed:
        exit(1)
