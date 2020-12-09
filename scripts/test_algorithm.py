from .diff_sizes import update_test_case_diff
from .gen_random_input import generate_and_save_test_case
from . import run_algorithm
import multiprocessing
from termcolor import colored
import numpy as np
import argparse
from copy import deepcopy

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
    default=None,
    help="number of processes to run our distributed algorithm with",
)
parser.add_argument(
    "--max-input-len",
    type=int,
    default=5000,
    help="largest length_1 to use when generating input files",
)
parser.add_argument(
    "--limit-programs",
    type=str,
    help="comma separated list of programs to benchmark",
)
parser.add_argument(
    "--speedup-baseline",
    type=str,
    default="sequential_frontier",
    help="name of program to use as baseline for calculating speedup",
)
parser.add_argument(
    "--speedup-improved",
    type=str,
    default="mpi_priority_frontier",
    help="name of program to use as fast version for calculating speedup",
)


def gen_random_generation_config(max_input_len):
    config = {
        "strategy": np.random.choice(
            ["independent", "add", "remove", "addremove"], p=[0.1, 0.2, 0.2, 0.5]
        ),
        "length_1": np.random.randint(1, np.random.randint(1000, max_input_len)),
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

    diff_programs = [
        program
        for program in run_algorithm.sequential_diff_programs
        if program["name"] != "diffutils"
    ]
    for program_template in run_algorithm.mpi_diff_programs:
        program = deepcopy(program_template)
        assert "extra_fields" not in "program"
        program["extra_fields"] = {"mpi_procs": args.mpi_procs}
        diff_programs.append(program)
    if args.limit_programs is not None:
        diff_programs = run_algorithm.limit_diff_programs(
            diff_programs,
            args.limit_programs,
            "unknown program names passed to --limit-programs",
        )

    some_tests_failed = False
    for i in range(args.num_tests):
        if i > 0:
            print()
        print(i, end=" ", flush=True)

        config = gen_random_generation_config(args.max_input_len)
        test_case_dir = generate_and_save_test_case(
            config, temporary=True, detailed_name=False
        )
        print(test_case_dir.name, config, flush=True)

        print("Running golden implementation", flush=True)
        golden_diff_size = update_test_case_diff(test_case_dir)

        speedup_baseline_output = None
        speedup_improved_output = None
        for program in diff_programs:
            print(f"Running {program['name']}", flush=True, end=" ")
            try:
                output = program["run"](
                    test_case_dir / "in_1.txt",
                    test_case_dir / "in_2.txt",
                    program.get("extra_fields", {}),
                )
            except Exception as e:
                print(e)
                print(f'{colored("FAIL", "red")} exception raised')
                continue

            print(f"[{output.micros_until_len} μs]", flush=True, end=" ")

            if output.min_edit_len == golden_diff_size:
                print(colored("PASS", "green"))
            else:
                print(
                    f'{colored("FAIL", "red")} want {golden_diff_size} got {output.min_edit_len}'
                )
                some_tests_failed = True
                if args.early_stop:
                    break

            if program["name"] == args.speedup_baseline:
                speedup_baseline_output = output
            if program["name"] == args.speedup_improved:
                speedup_improved_output = output

        if speedup_baseline_output is not None and speedup_improved_output is not None:
            print(
                f"Speed-up: {speedup_baseline_output.micros_until_len/speedup_improved_output.micros_until_len:.2f}x ({args.speedup_baseline} / {args.speedup_improved})"
            )

    if some_tests_failed:
        exit(1)
