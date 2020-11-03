import os
import sys
import subprocess
from subprocess import check_call
from termcolor import colored

test_case_folder = "../test_cases/"
diff_filename = "diff_size.txt"
diff_command = 'diff --minimal in_1.txt in_2.txt | egrep -c "^[<>]" > diff_size.txt'
always_create_diff = False

help_msg = """
Input Arguments
default:  generate missing diffs
all:      generate all diffs again
"""
if len(sys.argv) > 1:
    arg = sys.argv[1]
    if arg.lower() == "all":
        always_create_diff = True
    else:
        print(help_msg)
        exit(1)


# go through test_case folders
counter = 0
print(f"{'Folder':<18}Edit Distance")
for folder in os.scandir(test_case_folder):
    if folder.is_dir():

        diff_path = folder.path + "/" + diff_filename
        diff_exists = os.path.isfile(diff_path)

        # create diff file
        if always_create_diff or not diff_exists:
            try:
                result = check_call(diff_command, shell=True, cwd=folder.path)
                f = open(diff_path, "r")
                result = f.read().rstrip()
                f.close()
                counter += 1
                print(f"{folder.name:<18}{result}")

            # delete incorrect file and show error
            except subprocess.CalledProcessError as e:
                os.remove(diff_path)
                print(f"{folder.name:<18}{colored('Failed', 'red')} to generate diff")

print(f"\nGenerated {counter} Files")