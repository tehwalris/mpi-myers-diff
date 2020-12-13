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
import shlex
from scipy.stats import binom
import bisect
from subprocess import TimeoutExpired

default_bench_dir = Path(__file__).parent / "../test_cases/temp_bench"
flush_every_seconds = 5

parser = argparse.ArgumentParser(
    description="Benchmark our diff algorithm with random test cases"
)
subparsers = parser.add_subparsers(title="subcommands")
parser_prepare = subparsers.add_parser("prepare")
parser_plan_batch = subparsers.add_parser("plan-batch")
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

parser_plan_batch.add_argument(
    "--output-dir",
    type=Path,
    required=True,
    help="path to directory where to write the benchmark results as multiple CSV files",
)
parser_plan_batch.add_argument(
    "--job-start-command-format",
    default=r"bsub -n %procs% %command%",
    help=r"command for starting a single batch job. %procs% and %command% will be replaced.",
)
parser_plan_batch.add_argument(
    "--paths-relative-to",
    type=Path,
    default=Path.cwd(),
    help="refer to programs and files using paths relative to the supplied path",
)
parser_plan_batch.add_argument(
    "--mpi-procs",
    type=int,
    nargs="+",
    required=True,
    help="number of processes to run our distributed algorithm with. If multiple (space separated) numbers are supplied, every MPI program is benchmarked for each.",
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
parser_run.add_argument(
    "--no-direct-mpi-procs-limit",
    default=False,
    action="store_true",
    help="don't pass -np to MPI run. Instead just assume that the number of processes is being limited to --mpi-procs somehow externally (eg. by a batch system). --mpi-procs is then effectively only written to the output CSV.",
)
parser_run.add_argument(
    "--mpi-procs",
    type=int,
    nargs="+",
    default=[None],
    help="number of processes to run our distributed algorithm with. If multiple (space separated) numbers are supplied, every MPI program is benchmarked for each.",
)

plan_batch_run_shared_args = set()


def add_plan_batch_run_shared_argument(arg_name, *args, **kwargs):
    assert arg_name.startswith("--")
    arg_name_snake = arg_name[2:].replace("-", "_")
    plan_batch_run_shared_args.add(arg_name_snake)
    for p in parser_plan_batch, parser_run:
        p.add_argument(arg_name, *args, **kwargs)


add_plan_batch_run_shared_argument(
    "--input-dir",
    type=Path,
    default=default_bench_dir,
    help="path to directory where to read the generated inputs for benchmarking",
)
add_plan_batch_run_shared_argument(
    "--limit-programs",
    type=str,
    help="comma separated list of programs to benchmark",
)
add_plan_batch_run_shared_argument(
    "--auto-repetitions",
    default=False,
    action="store_true",
    help="Determine number of repitions automatically based on the confidence interval around the median",
)
add_plan_batch_run_shared_argument(
    "--min-repetitions",
    type=int,
    default=5,
    help="minimum number of times to re-run each diff program with exactly the same input",
)
add_plan_batch_run_shared_argument(
    "--max-repetitions",
    type=int,
    default=1000,  # "infinity"
    help="maximum number of times to re-run each diff program with exactly the same input, only active if --auto-repetitions is given",
)  # TODO: needs to be >= 5?
add_plan_batch_run_shared_argument(
    "--max-median-error",
    type=float,
    default=0.05,  # TODO: ?
    help="maximal relative error of the median in the confidence interval, only active if --auto-repetitions is given",
)
confidence_level = 0.95  # TODO:
add_plan_batch_run_shared_argument(
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


def get_diff_programs_for_args(args):
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

    return diff_programs


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


def plan_batch_benchmark(args):
    diff_programs = get_diff_programs_for_args(args)

    assert r"%procs%" in args.job_start_command_format
    assert r"%command%" in args.job_start_command_format

    def path_to_str(p):
        return str(p.resolve().relative_to(args.paths_relative_to.absolute()))

    def join_command(split_command):
        return " ".join(shlex.quote(arg) for arg in split_command)

    job_start_commands = []
    for program in diff_programs:
        mpi_procs = program.get("extra_fields", {}).get("mpi_procs", None)
        batch_procs = 1 if mpi_procs is None else mpi_procs
        assert isinstance(batch_procs, int)

        bench_command = [
            "python",
            "-m",
            "scripts.bench_algorithm",
            "run",
            "--input-dir",
            path_to_str(args.input_dir),
            "--num-repetitions",  # TODO
            str(args.num_repetitions),
            "--limit-programs",
            program["name"],
            "--no-direct-mpi-procs-limit",
            "--output-csv",
            path_to_str(args.output_dir / f'{program["name"]}_{batch_procs}.csv'),
        ]

        if mpi_procs is not None:
            bench_command += [
                "--mpi-procs",
                str(mpi_procs),
            ]

        if args.verbose:
            bench_command.append("--verbose")

        job_start_command = args.job_start_command_format.replace(
            r"%procs%",
            str(batch_procs),
        ).replace(
            r"%command%",
            shlex.quote(join_command(bench_command)),
        )

        job_start_commands.append(job_start_command)

    print(join_command(["mkdir", "-p", path_to_str(args.output_dir)]))
    for job_start_command in job_start_commands:
        print(job_start_command)


def run_benchmark(args):
    def verbose_print(*a, **kw):
        if args.verbose:
            print(*a, **kw)

    diff_programs = get_diff_programs_for_args(args)

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
        num_regens,
        len(diff_programs),
        # args.num_repetitions,  # TODO
    ]
    total_test_combinations = np.prod(test_combination_factors)
    print(
        f'{" * ".join(str(v) for v in test_combination_factors)} = {total_test_combinations} total test combinations'
    )

    csv_output_file = open(args.output_csv, "w", newline="")
    csv_output_writer = CSVOutputWriter(csv_output_file)

    def get_extra_file_path(suffix):
        name = ".".join(args.output_csv.name.split(".")[:-1]) + suffix
        return args.output_csv.parent / name

    failed_file_path = get_extra_file_path("-FAILED.txt")
    failed_file = open(failed_file_path, "w")

    # TODO: compute indices via Bin for different n until max
    if args.auto_repetitions:
        num_repetitions = args.max_repetitions
    else:
        num_repetitions = args.min_repetitions

    if args.no_progress_bar:
        progress_bar = NoopProgressBar()
    else:
        progress_bar = tqdm(total=total_test_combinations, smoothing=0)

    last_flush_time = time.monotonic()
    break_flag = False
    some_benchmarks_failed = False
    for _entry in shuffled_generation_configs:
        generation_config_i = _entry["i"]
        generation_config = _entry["config"]

        for regen_i in range(num_regens):
            verbose_print("generation_config", generation_config)
            test_case_dir = (
                args.input_dir / f"config-{generation_config_i}-regen-{regen_i}"
            )

            for diff_program in diff_programs:

                # sorted list of measurements
                micros_until_len_res = []

                for repetition_i in range(num_repetitions):
                    if time.monotonic() - last_flush_time > flush_every_seconds:
                        csv_output_file.flush()
                        failed_file.flush()
                        last_flush_time = time.monotonic()

                    verbose_print("  diff_program", diff_program["name"])

                    extra_fields_for_output = {
                        k: diff_program.get("extra_fields", {}).get(k, "")
                        for k in all_diff_program_extra_fields
                    }

                    extra_fields_for_run = deepcopy(
                        diff_program.get("extra_fields", {})
                    )
                    if (
                        args.no_direct_mpi_procs_limit
                        and "mpi_procs" in extra_fields_for_run
                    ):
                        extra_fields_for_run["mpi_procs"] = None

                    try:
                        program_result = diff_program["run"](
                            test_case_dir / "in_1.txt",
                            test_case_dir / "in_2.txt",
                            extra_fields_for_run,
                        )
                        verbose_print(
                            "    micros_until_len", program_result.micros_until_len
                        )
                    except KeyboardInterrupt:  # exit the benchmark
                        break_flag = True
                        break
                    except TimeoutExpired as te:
                        some_benchmarks_failed = True
                        print(diff_program["name"] + "\t", file=failed_file, end="")
                        print(generation_config, file=failed_file, end="")
                        print("\t" + repr(te), file=failed_file)
                        if args.auto_repetitions:
                            timeout_micros = te.timeout * 1e6  # seconds to microseconds
                            if (
                                repetition_i >= 5
                                and micros_until_len_res[0] == timeout_micros
                            ):
                                break  # if five iterations timed out -> assume all will timeout, don't try again
                            micros_until_len_res.append(timeout_micros)  # TODO:?
                        continue
                    except Exception as e:  # catch all
                        some_benchmarks_failed = True
                        print(diff_program["name"] + "\t", file=failed_file, end="")
                        print(generation_config, file=failed_file, end="")
                        print("\t" + repr(e), file=failed_file)
                        break  # assumption: will always fail with these exceptions -> no need to run all repetitions

                    output_data = {
                        "generation_config_i": generation_config_i,
                        **{f"input_{k}": v for k, v in generation_config.items()},
                        "regen_i": regen_i,
                        "repetition_i": repetition_i,
                        "diff_program": diff_program["name"],
                        **extra_fields_for_output,
                        "mpi_comm_world": getattr(program_result, "mpi_comm_world", 1),
                        "micros_input": program_result.micros_input,
                        "micros_precompute": program_result.micros_precompute,
                        "micros_until_len": program_result.micros_until_len,
                        "micros_edit_script": program_result.micros_edit_script,
                        "min_edit_length": program_result.min_edit_length,
                    }

                    csv_output_writer.write_row(output_data)

                    if args.auto_repetitions:
                        bisect.insort(
                            micros_until_len_res, program_result.micros_until_len
                        )
                        if (
                            repetition_i >= args.min_repetitions and repetition_i >= 5
                        ):  # TODO: 5 for 0.95, 7 for 0.99
                            # check if required confidence interval is reached
                            if repetition_i % 2 == 0:  # odd number of results
                                current_median = micros_until_len_res[repetition_i // 2]
                            else:
                                current_median = (
                                    micros_until_len_res[(repetition_i - 1) // 2]
                                    + micros_until_len_res[(repetition_i + 1) // 2]
                                ) / 2

                            lower_idx, upper_idx = binom.interval(
                                confidence_level, repetition_i + 1, 0.5
                            )
                            # to get correct indices in python (Boudec paper Appendix A - 1)
                            lower_idx -= 1
                            # sometimes the interval is a little bit wider than in the Boudec paper, but this just means more confidence

                            if (
                                micros_until_len_res[int(lower_idx)]
                                >= (1 - args.max_median_error) * current_median
                                and micros_until_len_res[int(upper_idx)]
                                <= (1 + args.max_median_error) * current_median
                            ):
                                break

                            if repetition_i == num_repetitions - 1:
                                # failed to reach required confidence
                                some_benchmarks_failed = True
                                print(
                                    diff_program["name"] + "\t",
                                    file=failed_file,
                                    end="",
                                )
                                print(generation_config, file=failed_file, end="")
                                print(
                                    "\t"
                                    + f"Failed to reach required confidence after {num_repetitions} repetitions; "
                                    + f"current median: {current_median}, left end of CI: {micros_until_len_res[int(lower_idx)]}, right end of CI: {micros_until_len_res[int(upper_idx)]}",
                                    file=failed_file,
                                )

                progress_bar.update()

                if break_flag:
                    break
            if break_flag:
                break
        if break_flag:
            break

    progress_bar.close()
    csv_output_file.close()
    failed_file.close()

    if not some_benchmarks_failed:
        failed_file_path.unlink()


parser_prepare.set_defaults(func=prepare_benchmark)
parser_run.set_defaults(func=run_benchmark)
parser_plan_batch.set_defaults(func=plan_batch_benchmark)

if __name__ == "__main__":
    if len(sys.argv) == 1:
        print("subcommand is required")
        parser.print_usage()
        exit(1)
    args = parser.parse_args()
    args.func(args)