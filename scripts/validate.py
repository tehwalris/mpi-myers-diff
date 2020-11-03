import os
import os.path
import sys
import subprocess
from pathlib import Path
from termcolor import colored
from subprocess import check_call


test_case_folder = "../test_cases/"
edit_filename = "edit_script.txt"
diff_filename = "diff_size.txt"


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

def validate_diff_size(folder, diff):
    with open(folder.path + "/" + edit_filename, "rb") as edit:
        
        actual = sum(1 for i in edit)
        expected = int(diff.readline().strip())
        if actual == expected:
            print(f"{colored('OK', 'green'):<25}")
        else:
            print(f"Expected {expected} but got {colored(actual, 'red'):<25}")


def validate_test(folder):
    with open(folder.path + "/in_1.txt", "r") as in1, \
        open(folder.path + "/in_2.txt", "r") as in2, \
        open(folder.path + "/out.txt", "w") as out, \
        open(folder.path + "/" + diff_filename, "r") as diff, \
        open(folder.path + "/" + edit_filename, "r") as edit:

        apply_edit_script(in1, in2, edit, out)

        # compare output
        try:
            result = check_call("cmp out.txt in_2.txt", shell=True, cwd=folder.path)
            print(f"{folder.name:<18}{colored('OK', 'green'):<25}\t", end='')
            validate_diff_size(folder, diff)

        except subprocess.CalledProcessError as e:
            print(f"{folder.name:<18}{colored('Failed', 'red')}")


def main():

    # go trough all test cases
    print(f"{'Folder':<18}{'Output correct':<20}\t{'Diff size correct':<18}")
    for folder in os.scandir(test_case_folder):
        if folder.is_dir():

            edit_path = folder.path + "/" + edit_filename
            edit_exists = os.path.isfile(edit_path)

            # edit script exists -> validate
            if edit_exists:
                validate_test(folder)
            else:
                print(f"{folder.name:<18}No edit script")

if __name__ == "__main__":
    main()