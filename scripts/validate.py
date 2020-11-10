import os
import sys
import subprocess
from pathlib import Path
from termcolor import colored
from subprocess import check_call

test_case_folder = Path(__file__).parent / "../test_cases"
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
            return True
        else:
            print(f"Expected {expected} but got {colored(actual, 'red'):<25}")
            return False



# return 2 booleans:
# edit_script corrent?, diff size correct?
def validate_test(folder):
    with open(folder.path + "/in_1.txt", "r") as in1, \
        open(folder.path + "/in_2.txt", "r") as in2, \
        open(folder.path + "/out.txt", "w") as out, \
        open(folder.path + "/" + diff_filename, "r") as diff, \
        open(folder.path + "/" + edit_filename, "r") as edit:

        apply_edit_script(in1, in2, edit, out)

        # compare output
        diff_size_correct = False
        output_correct = False
        try:
            result = check_call("cmp --silent out.txt in_2.txt", shell=True, cwd=folder.path)
            print(f"{folder.name:<20}{colored('OK', 'green'):<25}\t", end='')
            output_correct = True
            diff_size_correct = validate_diff_size(folder, diff)

        except subprocess.CalledProcessError as e:
            print(f"{folder.name:<20}{colored('Failed', 'red')}")
        
        return output_correct, diff_size_correct


##
# Input Arguments
# default:           apply the edit scripts and validate that the output matches
# folder names..:    only validate the test case #name#

def main():

    use_arg_folders = False
    if len(sys.argv) > 1:
        arg_folder = sys.argv[1]
        use_arg_folders = True

    # go trough all test cases
    print(f"{'Folder':<20}{'Output correct':<18}\t{'Diff size correct':<18}")
    for folder in os.scandir(test_case_folder):
        if folder.is_dir():
            if use_arg_folders and not folder.name in sys.argv[1:]:
                continue

            edit_path = folder.path + "/" + edit_filename
            edit_exists = os.path.isfile(edit_path)

            # edit script exists -> validate
            if edit_exists:
                validate_test(folder)
            else:
                print(f"{folder.name:<20}No edit script")

if __name__ == "__main__":
    main()