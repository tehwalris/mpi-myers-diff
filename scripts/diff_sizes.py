import os
import sys
import subprocess
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
        result = subprocess.run(
            diff_command,
            shell=True,
            cwd=test_case_folder_path,
            stdout=subprocess.DEVNULL,
        )
        if result.returncode not in [0, 1]:
            raise RuntimeError(f"diff command returned {result.returncode}")
    except subprocess.CalledProcessError as e:
        os.remove(diff_path)
        raise e

    with open(diff_path, "r") as f:
        return int(f.read().rstrip())


class TestCaseDir:
    def __init__(self, name, path):
        self.name = name
        self.path = path

def get_test_case_dirs(search_root=test_case_folder):
    for folder in os.scandir(search_root):
        if folder.is_dir():
            if (Path(folder.path) / "in_1.txt").is_file():
                yield TestCaseDir(folder.name, folder.path)
            for subfolder in get_test_case_dirs(folder.path):
                yield TestCaseDir(f'{folder.name}/{subfolder.name}', subfolder.path)


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