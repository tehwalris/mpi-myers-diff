import os
import sys
import subprocess
from subprocess import check_call
from termcolor import colored
import traceback
from pathlib import Path

test_case_folder = Path(__file__).parent / "../test_cases/"
diff_filename = "diff_size.txt"
diff_command = 'diff --minimal in_1.txt in_2.txt | egrep -c "^[<>]" > diff_size.txt'
help_msg = """
Input Arguments
default:  generate missing diffs
all:      generate all diffs again
"""


def update_test_case_diff(test_case_folder_path, always_create_diff=True):
    diff_path = Path(test_case_folder_path) / diff_filename
    diff_exists = os.path.isfile(diff_path)

    if diff_exists and not always_create_diff:
        return None

    try:
        check_call(diff_command, shell=True, cwd=test_case_folder_path)
    except subprocess.CalledProcessError as e:
        os.remove(diff_path)
        raise e

    with open(diff_path, "r") as f:
        return int(f.read().rstrip())


def get_test_case_dirs():
    for folder in os.scandir(test_case_folder):
        if folder.is_dir():
            yield folder


if __name__ == "__main__":
    always_create_diff = False
    if len(sys.argv) > 1:
        arg = sys.argv[1]
        if arg.lower() == "all":
            always_create_diff = True
        else:
            print(help_msg)
            exit(1)

    counter = 0
    print(f"{'Folder':<50}Edit Distance")
    for folder in get_test_case_dirs():
        try:
            result = update_test_case_diff(folder.path, always_create_diff)
        except Exception:
            traceback.print_exc()
            print(f"{folder.name:<50}{colored('Failed', 'red')} to generate diff")
            continue

        if result is not None:
            counter += 1
            print(f"{folder.name:<50}{result}")

    print(f"\nGenerated {counter} Files")