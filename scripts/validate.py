import os
import sys
import subprocess
import numpy as np
from pathlib import Path
from termcolor import colored
from subprocess import check_call
import argparse
from .diff_sizes import update_test_case_diff
from .gen_random_input import generate_and_save_test_case

test_case_folder = Path(__file__).parent / "../test_cases"
edit_filename = "edit_script.txt"
diff_filename = "diff_size.txt"

parser = argparse.ArgumentParser(
    description="Applies the edit script of a test case to the first input file and then checks that the output matches the second input file. Additionally, it checks that the size of the diff matches what diff --minimal provides"
)
parser.add_argument(
    "--num-rand-tests", type=int, default=0, help="number of randomized tests to run"
)
# parser.add_argument(
#     "--early-stop",
#     default=False,
#     action="store_true",
#     help="skip running further tests as soon as one fails",
# )
parser.add_argument(
    "--mpi-procs",
    help="number of processes to run a distributed algorithm with",
)
parser.add_argument(
    "--regen-with",
    help="Regenerates the edit scripts with the provided diff executable"
)
parser.add_argument(
    "test_cases", 
    nargs="*",
    help="Provide a list of test_case folder names if only specific test cases should be checked. Otherwise all test cases are evaluated."
)

def gen_random_generation_config():
    config = {
        "strategy": np.random.choice(
            ["independent", "add", "remove", "addremove"], p=[0.1, 0.2, 0.2, 0.5]
        ),
        "length_1": np.random.randint(1, 10 ** np.random.randint(1, 5)),
        "change_strength": np.random.rand()
        * np.random.choice([0.3, 1], p=[0.75, 0.25]),
        "chunkiness": np.random.rand(),
        "distribution": np.random.choice(["uniform", "zipf"], p=[0.25, 0.75]),
    }
    if config["strategy"] == "independent":
        config["change_strength"] = 1
        config["chunkiness"] = 0
    return config


def apply_edit_script(in1, in2, edit, out):
    output_lines = []
    current_in1_index = 0

    append_output = output_lines.append

    line = edit.readline().strip()
    while line != "":
        edit_line = line.split(" ")

        edit_index = int(edit_line[0])
        edit_op = edit_line[1]

        if edit_index > current_in1_index:
            # copy common content from in_1
            for i in range(edit_index - current_in1_index):
                append_output(in1.readline())
            current_in1_index = edit_index

        if edit_op == "-":
            output_lines.pop()
        else:
            append_output(edit_line[2] + "\n")

        line = edit.readline().strip()  # continue while loop

    # copy final common block
    while True:
        line = in1.readline()
        if not line:
            break
        output_lines.append(line)

    out.write("".join(output_lines))
    out.close()

def validate_diff_size(path_str, diff):
    with open(path_str + "/" + edit_filename, "rb") as edit:
        
        actual = sum(1 for i in edit)
        expected = int(diff.readline().strip())
        if actual == expected:
            print(f"{colored('OK', 'green'):<25}")
            return True
        else:
            print(f"Expected {expected} but got {colored(actual, 'red'):<25}")
            return False



# return 2 booleans:
# edit_script corrent?, diff size correct?
def validate_test(path_str, test_case):
    with open(path_str + "/in_1.txt", "r") as in1, \
        open(path_str + "/in_2.txt", "r") as in2, \
        open(path_str + "/out.txt", "w") as out, \
        open(path_str + "/" + diff_filename, "r") as diff, \
        open(path_str + "/" + edit_filename, "r") as edit:

        apply_edit_script(in1, in2, edit, out)

        # compare output
        diff_size_correct = False
        output_correct = False
        try:
            result = check_call("cmp --silent out.txt in_2.txt", shell=True, cwd=path_str)
            print(f"{test_case:<20}{colored('OK', 'green'):<25}\t", end='')
            output_correct = True
            diff_size_correct = validate_diff_size(path_str, diff)

        except subprocess.CalledProcessError as e:
            print(f"{test_case:<20}{colored('Failed', 'red')}")
        
        return output_correct, diff_size_correct


def main():

    args = parser.parse_args()

    # go through all test cases
    print(f"{'Folder':<20}{'Output correct':<18}\t{'Diff size correct':<18}")
    for folder in os.scandir(test_case_folder):
        if folder.is_dir():
            if args.test_cases and not folder.name in args.test_cases:
                continue
            if args.num_rand_tests and folder.name == "temp_random":
                continue

            edit_path = folder.path + "/" + edit_filename

            if args.regen_with:
                run_args = []
                if args.mpi_procs:
                    # run with MPI
                    run_args += ["mpiexec", "-np", args.mpi_procs]
                run_args += [args.regen_with, folder.path + "/in_1.txt", folder.path + "/in_2.txt", edit_path]
                try:
                    subprocess.run(run_args, check=True, capture_output=True, timeout=5.)
                except subprocess.CalledProcessError as e:
                    print(f"{folder.name:<20}{colored('Failed', 'red')}: Execution of diff algorithm failed with exit code {e.returncode} and stderr: \n {e.stderr}")
                    continue
                except subprocess.TimeoutExpired as e:
                    print(f"{folder.name:<20}{colored('Failed', 'red')}: Execution of diff algorithm timed out after {e.timeout} seconds.")
                    continue

            edit_exists = os.path.isfile(edit_path)
            
            diff_path = folder.path + "/" + diff_filename
            diff_exists = os.path.isfile(diff_path)
            if not diff_exists:
                # generate diff
                update_test_case_diff(folder.path)

            # edit script exists -> validate
            if edit_exists:
                validate_test(folder.path, folder.name)
            else:
                print(f"{folder.name:<20}No edit script")


    # run random tests
    if args.num_rand_tests > 0:
        if not args.regen_with:
            print(f"Cannot run random test cases since no executable was provided with --regen-with")
            exit(1)

    for i in range(args.num_rand_tests):
        print(i, end=" ", flush=True)

        config = gen_random_generation_config()
        test_case_dir = generate_and_save_test_case(
            config, temporary=True, detailed_name=False
        )
        print(test_case_dir.name, config, flush=True)

        update_test_case_diff(test_case_dir)

        edit_path = test_case_dir / edit_filename

        run_args = []
        if args.mpi_procs:
            # run with MPI
            run_args += ["mpiexec", "-np", args.mpi_procs]
        run_args += [args.regen_with, test_case_dir / "in_1.txt", test_case_dir / "in_2.txt", edit_path]
        try:
            subprocess.run(run_args, check=True, capture_output=True, timeout=5.)
        except subprocess.CalledProcessError as e:
            print(f"random test {i} {colored('Failed', 'red')}: Execution of diff algorithm failed with exit code {e.returncode} and stderr: \n {e.stderr}")
            continue
        except subprocess.TimeoutExpired as e:
            print(f"random test {i} {colored('Failed', 'red')}: Execution of diff algorithm timed out after {e.timeout} seconds.")
            continue

        edit_exists = os.path.isfile(edit_path)

        # edit script exists -> validate
        if edit_exists:
            validate_test(str(test_case_dir), f"random test {i}")
        else:
            print(f"random test {i} No edit script")


if __name__ == "__main__":
    main()