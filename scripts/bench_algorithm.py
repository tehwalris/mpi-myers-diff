from .gen_random_input import generate_and_save_test_case
from . import run_algorithm
import multiprocessing
import numpy as np
import argparse
import csv
import random
from tqdm import tqdm

parser = argparse.ArgumentParser(
    description="Benchmark our diff algorithm with random test cases"
)
parser.add_argument(
    "--output-csv",
    type=str,
    required=True,
    help="path where to write the benchmark results as a CSV file",
)
parser.add_argument(
    "--min-file-size",
    type=int,
    default=1,
    help="minimum length of a single generated input sequence",
)
parser.add_argument(
    "--max-file-size",
    type=int,
    default=5000,
    help="maximum length of a single generated input sequence",
)
parser.add_argument(
    "--target-file-size-steps",
    type=int,
    default=10,
    help="number of different file sizes to try while keeping all other parameters fixed",
)
parser.add_argument(
    "--target-change-strength-steps",
    type=int,
    default=5,
    help="number of different change strengths to try while keeping all other parameters fixed",
)
parser.add_argument(
    "--target-chunkiness-steps",
    type=int,
    default=3,
    help="number of different chunkiness levels to try while keeping all other parameters fixed",
)
parser.add_argument(
    "--mpi-procs",
    type=int,
    default=multiprocessing.cpu_count(),
    help="number of processes to run our distributed algorithm with",
)
parser.add_argument(
    "--num-repetitions",
    type=int,
    default=3,
    help="number of times to re-run each diff program with exactly the same input",
)
parser.add_argument(
    "--num-regens",
    type=int,
    default=3,
    help="number of times to re-generate an input using the same config",
)
parser.add_argument(
    "--limit-programs",
    type=str,
    help="comma separated list of programs to benchmark",
)
parser.add_argument(
    "--verbose",
    default=False,
    action="store_true",
    help="print detailed information when running each benchmark",
)


class CSVOutputWriter:
    def __init__(self, file):
        self._file = file
        self._writer = None

    def write_row(self, data: dict):
        if self._writer is None:
            self._writer = csv.DictWriter(self._file, data.keys())
            self._writer.writeheader()
        self._writer.writerow(data)


if __name__ == "__main__":
    args = parser.parse_args()

    def verbose_print(*a, **kw):
        if args.verbose:
            print(*a, **kw)

    file_size_steps = np.geomspace(
        args.min_file_size, args.max_file_size, args.target_file_size_steps
    )
    file_size_steps = np.unique(np.floor(file_size_steps).astype(int))

    change_strength_steps = np.linspace(0, 1, args.target_change_strength_steps + 1)[1:]

    chunkiness_steps = np.linspace(0, 1, args.target_chunkiness_steps)

    all_generation_configs = []
    for file_size in file_size_steps:
        for change_strength in change_strength_steps:
            for chunkiness in chunkiness_steps:
                for generation_strategy in [
                    "independent",
                    "add",
                    "remove",
                    "addremove",
                ]:
                    if generation_strategy == "independent" and (
                        change_strength != 1 or chunkiness != 0
                    ):
                        continue

                    generation_config = {
                        "strategy": generation_strategy,
                        "length_1": file_size,
                        "change_strength": change_strength,
                        "chunkiness": chunkiness,
                        "distribution": "zipf",
                    }
                    all_generation_configs.append(generation_config)
    print(f"{len(all_generation_configs)} unique test cases")

    diff_programs = [
        {
            "name": "mpi_master",
            "run": lambda p1, p2: run_algorithm.run_diff_algorithm_mpi(
                p1, p2, args.mpi_procs, run_algorithm.own_diff_executable_mpi_master
            ),
            "extra_fields": {"mpi_procs": args.mpi_procs},
        },
        {
            "name": "mpi_no_master",
            "run": lambda p1, p2: run_algorithm.run_diff_algorithm_mpi(
                p1, p2, args.mpi_procs, run_algorithm.own_diff_executable_mpi_no_master
            ),
            "extra_fields": {"mpi_procs": args.mpi_procs},
        },
        {
            "name": "sequential",
            "run": lambda p1, p2: run_algorithm.run_own_diff_algorithm_sequential(
                p1, p2, run_algorithm.own_diff_executable_sequential
            ),
        },
        {
            "name": "sequential_fast_snakes",
            "run": lambda p1, p2: run_algorithm.run_own_diff_algorithm_sequential(
                p1, p2, run_algorithm.own_diff_executable_sequential_fast_snakes
            ),
        },
        {
            "name": "diffutils",
            "run": run_algorithm.run_diffutils,
        },
    ]

    if args.limit_programs is not None:
        possible_names = set(p["name"] for p in diff_programs)
        selected_names = set(s.strip() for s in args.limit_programs.split(","))

        unknown_names = selected_names - possible_names
        if unknown_names:
            raise ValueError(
                f'unknown program names passed to --limit-programs: {", ".join(sorted(unknown_names))}'
            )

        diff_programs = [p for p in diff_programs if p["name"] in selected_names]
        assert len(diff_programs) == len(selected_names)

    all_diff_program_extra_fields = sorted(
        {k for p in diff_programs for k in p.get("extra_fields", {}).keys()}
    )
    print(f"{len(diff_programs)} diff programs")

    test_combination_factors = [
        len(all_generation_configs),
        len(diff_programs),
        args.num_repetitions,
        args.num_regens,
    ]
    total_test_combinations = np.prod(test_combination_factors)
    print(
        f'{" * ".join(str(v) for v in test_combination_factors)} = {total_test_combinations} total test combinations'
    )

    csv_output_file = open(args.output_csv, "w", newline="")
    csv_output_writer = CSVOutputWriter(csv_output_file)

    progress_bar = tqdm(total=total_test_combinations, smoothing=0)

    shuffled_generation_configs = list(enumerate(all_generation_configs.copy()))
    random.shuffle(shuffled_generation_configs)
    for generation_config_i, generation_config in shuffled_generation_configs:
        for regen_i in range(args.num_regens):
            verbose_print("generation_config", generation_config)
            test_case_dir = generate_and_save_test_case(
                generation_config, temporary=True, detailed_name=False
            )

            for diff_program in diff_programs:
                for repetition_i in range(args.num_repetitions):
                    verbose_print("  diff_program", diff_program["name"])
                    # TODO Enforce a timeout
                    program_result = diff_program["run"](
                        test_case_dir / "in_1.txt",
                        test_case_dir / "in_2.txt",
                    )
                    verbose_print(
                        "    micros_until_len", program_result.micros_until_len
                    )

                    progress_bar.update()

                    output_data = {
                        "generation_config_i": generation_config_i,
                        **{f"input_{k}": v for k, v in generation_config.items()},
                        "regen_i": regen_i,
                        "repetition_i": repetition_i,
                        "diff_program": diff_program["name"],
                        **{
                            k: diff_program.get("extra_fields", {}).get(k, "")
                            for k in all_diff_program_extra_fields
                        },
                        "micros_input": program_result.micros_input,
                        "micros_precompute": program_result.micros_precompute,
                        "micros_until_len": program_result.micros_until_len,
                        "micros_edit_script": program_result.micros_edit_script,
                    }
                    csv_output_writer.write_row(output_data)

    progress_bar.close()
    csv_output_file.close()