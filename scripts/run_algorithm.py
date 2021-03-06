from pathlib import Path
import subprocess
import re
import os
import signal
import time

own_diff_executable_sequential_measure_snakes = (
    Path(__file__).parent / "../bin/own-diff-sequential-measure-snakes.out"
)
diffutils_executable = Path(__file__).parent / "../bin/diffutils.out"


def _add_missing_fields_of_programs():
    for program in mpi_diff_programs + sequential_diff_programs:
        if "executable" not in program:
            program["executable"] = (
                Path(__file__).parent
                / f"../bin/own-diff-{program['name'].replace('_', '-')}.out"
            )

    def add_run_for_mpi_program(program):
        assert "run" not in program

        def run(p1, p2, e):
            return run_diff_algorithm_mpi(
                p1,
                p2,
                e["mpi_procs"],
                program["executable"],
                timeout_seconds=e.get("timeout_seconds", None),
            )

        program["run"] = run

    for program in mpi_diff_programs:
        if "run" not in program:
            # It's important that the "run" function created in a separate function,
            # otherwise you will have capturing issues.
            # https://stackoverflow.com/questions/2295290/what-do-lambda-function-closures-capture
            add_run_for_mpi_program(program)

    def add_run_for_sequential_program(program):
        assert "run" not in program

        def run(p1, p2, e):
            assert "mpi_procs" not in e
            return run_own_diff_algorithm_sequential(p1, p2, program["executable"])

        program["run"] = run

    for program in sequential_diff_programs:
        if "run" not in program:
            add_run_for_sequential_program(program)

    assert (
        len(
            set(p["name"] for p in mpi_diff_programs)
            & set(p["name"] for p in sequential_diff_programs)
        )
        == 0
    )


mpi_diff_programs = [
    {"name": "mpi_master"},
    {"name": "mpi_master_simd"},
    {"name": "mpi_no_master"},
    {"name": "mpi_no_master_simd"},
    {"name": "mpi_no_master_frontier"},
    {"name": "mpi_no_master_frontier_simd"},
    {"name": "mpi_priority"},
    {"name": "mpi_priority_simd"},
    {"name": "mpi_priority_frontier"},
    {"name": "mpi_priority_frontier_simd"},
]
sequential_diff_programs = [
    {"name": "sequential"},
    {"name": "sequential_simd"},
    {"name": "sequential_frontier"},
    {"name": "sequential_frontier_simd"},
    #    {"name": "sequential_fast_snakes"},
    {
        "name": "diffutils",
        "executable": diffutils_executable,
        "run": lambda p1, p2, _: run_diffutils(p1, p2),
    },
]
_add_missing_fields_of_programs()


def limit_diff_programs(diff_programs, limit_str, error_str):
    possible_names = set(p["name"] for p in diff_programs)
    selected_names = set(s.strip() for s in limit_str.split(","))

    unknown_names = selected_names - possible_names
    if unknown_names:
        raise ValueError(f'{error_str}: {", ".join(sorted(unknown_names))}')

    limited_diff_programs = [p for p in diff_programs if p["name"] in selected_names]
    assert len(diff_programs) >= len(selected_names)

    limited_diff_programs.sort(key=lambda p: p["name"])

    return limited_diff_programs


# https://stackoverflow.com/questions/32222681/how-to-kill-a-process-group-using-python-subprocess
def kill_process(pid):
    os.killpg(os.getpgid(pid), signal.SIGTERM)


def run_diff_algorithm_mpi(
    file_1_path,
    file_2_path,
    mpi_processes,
    mpi_executable_path,
    edit_script_path=None,
    min_entries=None,
    timeout_seconds=None,
):
    args = ["mpirun"]

    if mpi_processes is not None:
        args += ["-np", str(mpi_processes)]

    args += [
        mpi_executable_path,
        file_1_path,
        file_2_path,
    ]

    if edit_script_path is not None:
        args.append(edit_script_path)

    if min_entries is not None:
        args.append("-min_entries")
        args.append(str(min_entries))

    with subprocess.Popen(
        args,
        universal_newlines=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        start_new_session=True,
    ) as proc:
        try:
            all_output, error = proc.communicate(timeout=timeout_seconds)

        except Exception as e:
            # kill child and pass exception on to handle/log in benchmark
            # this means that KeyboardInterrupt properly kills both python AND the MPI processes
            proc.terminate()
            kill_process(proc.pid)  # kill children via process group
            time.sleep(5)
            proc.kill()
            proc.communicate()  # clear buffers
            raise

    return OwnDiffOutput(all_output)


def run_own_diff_algorithm_sequential(file_1_path, file_2_path, executable_path):

    args = [executable_path, file_1_path, file_2_path]

    with subprocess.Popen(
        args, universal_newlines=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE
    ) as proc:
        try:
            all_output, error = proc.communicate()

        except Exception as e:
            # kill child and pass exception on to handle/log in benchmark
            # this means that KeyboardInterrupt properly kills both python AND the processes
            proc.terminate()
            proc.kill()
            proc.communicate()
            raise

    return OwnDiffOutput(all_output)


def run_diffutils(file_1_path, file_2_path):

    args = [diffutils_executable, "--minimal", file_1_path, file_2_path]

    with subprocess.Popen(
        args, universal_newlines=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE
    ) as proc:
        all_output, error = proc.communicate()

        if proc.returncode not in [0, 1]:
            raise RuntimeError(f"diff command returned {proc.returncode}")
        return DiffutilsOutput(all_output)


def run_sequential_measure_snakes(file_1_path, file_2_path):
    all_output = subprocess.check_output(
        [own_diff_executable_sequential_measure_snakes, file_1_path, file_2_path],
        text=True,
    )
    return MeasureSnakesOutput(all_output)


class RegexExtractedOutput:
    def __init__(self, output: str, field_configs):
        found_fields = set()
        for field in field_configs:
            m = field["regex"].search(output)
            if m:
                assert field["key"] not in found_fields
                setattr(self, field["key"], field["extract"](m))
                found_fields.add(field["key"])

        missing_fields = {field["key"] for field in field_configs} - found_fields
        if missing_fields:
            print(output)
            raise ValueError(
                f"Fields not found in output: {', '.join(sorted(missing_fields))}"
            )


own_diff_output_fields = [
    {
        "key": "min_edit_length",
        "regex": re.compile(r"^min edit length (\d+)$", re.MULTILINE),
        "extract": lambda m: int(m[1]),
    },
    {
        "key": "micros_input",
        "regex": re.compile(r"^Read Input \[μs\]:\s*(\d+)$", re.MULTILINE),
        "extract": lambda m: int(m[1]),
    },
    {
        "key": "micros_precompute",
        "regex": re.compile(r"^Precompute \[μs\]:\s*(\d+)$", re.MULTILINE),
        "extract": lambda m: int(m[1]),
    },
    {
        "key": "micros_until_len",
        "regex": re.compile(r"^Solution \[μs\]:\s*(\d+)$", re.MULTILINE),
        "extract": lambda m: int(m[1]),
    },
    {
        "key": "micros_edit_script",
        "regex": re.compile(r"^Edit Script \[μs\]:\s*(\d+)$", re.MULTILINE),
        "extract": lambda m: int(m[1]),
    },
    {
        "key": "mpi_comm_world",
        "regex": re.compile(r"^mpi comm_world:\s*(\d+)$", re.MULTILINE),
        "extract": lambda m: int(m[1]),
    },
]


class OwnDiffOutput(RegexExtractedOutput):
    def __init__(self, output: str):
        super().__init__(output, own_diff_output_fields)


diffutils_output_fields = [
    {
        "key": "micros_input",
        "regex": re.compile(r"^Read Input \[μs\]:\s*(\d+)$", re.MULTILINE),
        "extract": lambda m: int(m[1]),
    },
    {
        "key": "micros_precompute",
        "regex": re.compile(r"^Precompute \[μs\]:\s*(\d+)$", re.MULTILINE),
        "extract": lambda m: int(m[1]),
    },
    {
        "key": "micros_until_len",
        "regex": re.compile(r"^Solution \[μs\]:\s*(\d+)$", re.MULTILINE),
        "extract": lambda m: int(m[1]),
    },
    {
        "key": "micros_edit_script",
        "regex": re.compile(r"^Edit Script \[μs\]:\s*(\d+)$", re.MULTILINE),
        "extract": lambda m: int(m[1]),
    },
]


class DiffutilsOutput(RegexExtractedOutput):
    def __init__(self, output: str):
        super().__init__(output, diffutils_output_fields)

        # add "min edit length" by counting its output:
        edit_len = len(re.findall(r"^[<>]", output, re.MULTILINE))
        setattr(self, "min_edit_length", edit_len)


class MeasureSnakesOutput:
    tuples = []

    def __init__(self, output: str):
        self.tuples = []
        for line in output.splitlines():
            if line.startswith("d,k,comparisons="):
                self.tuples.append(tuple(map(int, line.split("=")[1].split(","))))
