from .gen_random_input import generate_and_save_test_case, generate_input_pair
from . import run_algorithm
import multiprocessing
import numpy as np
import argparse
import csv
import random
from tqdm import tqdm
from copy import deepcopy
from pathlib import Path
import shutil
import json
import sys
import time

default_bench_dir = Path(__file__).parent / "../test_cases/temp_bench"
flush_every_seconds = 5

parser = argparse.ArgumentParser(
    description="Benchmark our diff algorithm with random test cases"
)
subparsers = parser.add_subparsers(required=True, title="subcommands")
parser_prepare = subparsers.add_parser("prepare")
parser_queue = subparsers.add_parser("queue")
parser_run = subparsers.add_parser("run")

parser_prepare.add_argument(
    "--min-file-size",
    type=int,
    default=1,
    help="minimum length of a single generated input sequence",
)
parser_prepare.add_argument(
    "--max-file-size",
    type=int,
    default=5000,
    help="maximum length of a single generated input sequence",
)
parser_prepare.add_argument(
    "--target-file-size-steps",
    type=int,
    default=10,
    help="number of different file sizes to try while keeping all other parameters fixed",
)
parser_prepare.add_argument(
    "--target-change-strength-steps",
    type=int,
    default=5,
    help="number of different change strengths to try while keeping all other parameters fixed",
)
parser_prepare.add_argument(
    "--only-change-strength",
    type=float,
    default=None,
    help="only change strength to benchmark with (--target-change-strength-steps must be 1 to use this option)",
)
parser_prepare.add_argument(
    "--target-chunkiness-steps",
    type=int,
    default=3,
    help="number of different chunkiness levels to try while keeping all other parameters fixed",
)
parser_prepare.add_argument(
    "--num-regens",
    type=int,
    default=3,
    help="number of times to re-generate an input using the same config",
)
parser_prepare.add_argument(
    "--output-dir",
    type=Path,
    default=default_bench_dir,
    help="path to directory where to write the generated inputs for benchmarking",
)

parser_queue.add_argument(
    "--output-csv-dir",
    type=Path,
    required=True,
    help="path to directory where to write the benchmark results as multiple CSV files",
)

parser_run.add_argument(
    "--output-csv",
    type=Path,
    required=True,
    help="path where to write the benchmark results as a CSV file",
)
parser_run.add_argument(
    "--no-progress-bar",
    default=False,
    action="store_true",
    help="hide the progress bar",
)

queue_run_shared_arguments = set()


def add_queue_run_shared_argument(arg_name, *args, **kwargs):
    assert arg_name.startswith("--")
    arg_name_snake = arg_name[2:].replace("-", "_")
    queue_run_shared_arguments.add(arg_name_snake)
    for p in parser_queue, parser_run:
        p.add_argument(arg_name, *args, **kwargs)


add_queue_run_shared_argument(
    "--input-dir",
    type=Path,
    default=default_bench_dir,
    help="path to directory where to read the generated inputs for benchmarking",
)
add_queue_run_shared_argument(
    "--limit-programs",
    type=str,
    help="comma separated list of programs to benchmark",
)
add_queue_run_shared_argument(
    "--mpi-procs",
    type=int,
    nargs="+",
    default=[multiprocessing.cpu_count()],
    help="number of processes to run our distributed algorithm with. If multiple (space separated) numbers are supplied, every MPI program is benchmarked for each.",
)
add_queue_run_shared_argument(
    "--num-repetitions",
    type=int,
    default=3,
    help="number of times to re-run each diff program with exactly the same input",
)
add_queue_run_shared_argument(
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


class NpEncoder(json.JSONEncoder):
    def default(self, obj):
        if isinstance(obj, np.integer):
            return int(obj)
        elif isinstance(obj, np.floating):
            return float(obj)
        elif isinstance(obj, np.ndarray):
            return obj.tolist()
        else:
            return super(NpEncoder, self).default(obj)


class NoopProgressBar:
    def update(*args, **kwargs):
        pass

    def close(*args, **kwargs):
        pass


def prepare_benchmark(args):
    file_size_steps = np.linspace(
        args.min_file_size, args.max_file_size, args.target_file_size_steps
    )
    file_size_steps = np.unique(np.floor(file_size_steps).astype(int))

    if args.only_change_strength is not None:
        assert args.target_change_strength_steps == 1
        change_strength_steps = np.array([args.only_change_strength])
    else:
        change_strength_steps = np.linspace(
            0, 1, args.target_change_strength_steps + 1
        )[1:]

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

    shuffled_generation_configs = list(
        {"i": i, "config": c} for i, c in enumerate(all_generation_configs)
    )
    random.shuffle(shuffled_generation_configs)

    shutil.rmtree(args.output_dir, ignore_errors=True)
    args.output_dir.mkdir()
    with (args.output_dir / "index.json").open("w", encoding="utf-8") as f:
        json.dump(
            {
                "shuffled_generation_configs": shuffled_generation_configs,
                "num_regens": args.num_regens,
            },
            f,
            cls=NpEncoder,
        )

    assert args.num_regens >= 1
    gen_combination_factors = [len(all_generation_configs), args.num_regens]
    total_gen_combinations = np.prod(gen_combination_factors)
    print(
        f'{" * ".join(str(v) for v in gen_combination_factors)} = {total_gen_combinations} total input file pairs'
    )

    progress_bar = tqdm(total=total_gen_combinations)
    for entry in shuffled_generation_configs:
        for regen_i in range(args.num_regens):
            test_case_dir = args.output_dir / f"config-{entry['i']}-regen-{regen_i}"
            test_case_dir.mkdir()
            for i, values in enumerate(generate_input_pair(**entry["config"])):
                with (test_case_dir / f"in_{i + 1}.txt").open(
                    "w", encoding="utf8"
                ) as f:
                    for v in values:
                        f.write(f"{v}\n")
            progress_bar.update()

    progress_bar.close()


def queue_benchmark(args):
    # TODO implement queueing
    pass


def run_benchmark(args):
    def verbose_print(*a, **kw):
        if args.verbose:
            print(*a, **kw)

    diff_programs = run_algorithm.sequential_diff_programs.copy()
    for program_template in run_algorithm.mpi_diff_programs:
        for mpi_procs in args.mpi_procs:
            program = deepcopy(program_template)
            assert "extra_fields" not in "program"
            program["extra_fields"] = {"mpi_procs": mpi_procs}
            diff_programs.append(program)

    if args.limit_programs is not None:
        diff_programs = run_algorithm.limit_diff_programs(
            diff_programs,
            args.limit_programs,
            "unknown program names passed to --limit-programs",
        )

    all_diff_program_extra_fields = sorted(
        {k for p in diff_programs for k in p.get("extra_fields", {}).keys()}
    )
    print(f"{len(diff_programs)} diff programs")

    try:
        with (args.input_dir / "index.json").open("r", encoding="utf-8") as f:
            benchmark_input_index = json.load(f)
    except Exception:
        print('Failed to load benchmark inputs. Did you run "prepare"?')
        raise
    shuffled_generation_configs = benchmark_input_index["shuffled_generation_configs"]
    num_regens = benchmark_input_index["num_regens"]
    assert num_regens >= 1

    test_combination_factors = [
        len(shuffled_generation_configs),
        len(diff_programs),
        num_regens,
    ]
    total_test_combinations = np.prod(test_combination_factors)
    print(
        f'{" * ".join(str(v) for v in test_combination_factors)} = {total_test_combinations} total test combinations'
    )

    csv_output_file = open(args.output_csv, "w", newline="")
    csv_output_writer = CSVOutputWriter(csv_output_file)

    def get_extra_file_path(suffix):
        return ".".join(args.output_csv.name.split(".")[:-1]) + suffix

    failed_file = open(get_extra_file_path("-FAILED.txt"), "w")

    if args.no_progress_bar:
        progress_bar = NoopProgressBar()
    else:
        progress_bar = tqdm(total=total_test_combinations, smoothing=0)

    last_flush_time = time.monotonic()
    break_flag = False
    for _entry in shuffled_generation_configs:
        generation_config_i = _entry["i"]
        generation_config = _entry["config"]

        for regen_i in range(num_regens):
            verbose_print("generation_config", generation_config)
            test_case_dir = (
                args.input_dir / f"config-{generation_config_i}-regen-{regen_i}"
            )

            for diff_program in diff_programs:
                for repetition_i in range(args.num_repetitions):
                    if time.monotonic() - last_flush_time > flush_every_seconds:
                        csv_output_file.flush()
                        failed_file.flush()
                        last_flush_time = time.monotonic()

                    verbose_print("  diff_program", diff_program["name"])

                    extra_fields = {
                        k: diff_program.get("extra_fields", {}).get(k, "")
                        for k in all_diff_program_extra_fields
                    }

                    try:
                        program_result = diff_program["run"](
                            test_case_dir / "in_1.txt",
                            test_case_dir / "in_2.txt",
                            extra_fields,
                        )
                        verbose_print(
                            "    micros_until_len", program_result.micros_until_len
                        )
                    except KeyboardInterrupt:  # exit the benchmark
                        break_flag = True
                        break
                    except Exception as e:  # catch all
                        progress_bar.update()

                        print(diff_program["name"] + "\t", file=failed_file, end="")
                        print(generation_config, file=failed_file, end="")
                        print("\t" + repr(e), file=failed_file)
                        continue

                    progress_bar.update()

                    output_data = {
                        "generation_config_i": generation_config_i,
                        **{f"input_{k}": v for k, v in generation_config.items()},
                        "regen_i": regen_i,
                        "repetition_i": repetition_i,
                        "diff_program": diff_program["name"],
                        **extra_fields,
                        "micros_input": program_result.micros_input,
                        "micros_precompute": program_result.micros_precompute,
                        "micros_until_len": program_result.micros_until_len,
                        "micros_edit_script": program_result.micros_edit_script,
                    }
                    csv_output_writer.write_row(output_data)

                if break_flag:
                    break
            if break_flag:
                break
        if break_flag:
            break

    progress_bar.close()
    csv_output_file.close()
    failed_file.close()


parser_prepare.set_defaults(func=prepare_benchmark)
parser_run.set_defaults(func=run_benchmark)
parser_queue.set_defaults(func=queue_benchmark)

if __name__ == "__main__":
    if len(sys.argv) == 1:
        print("subcommand is required")
        parser.print_usage()
        exit(1)
    args = parser.parse_args()
    args.func(args)