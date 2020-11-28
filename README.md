# 2020 DPHPC Project - Longest Common Subsequence

## Building diffutils

The build for diffutils is a little different than what their guide says, because diffutils is in a subdirectory inside our repo, not at the root.

### Initial setup

You only have to perform these steps once. After that, you can just perform the steps in the "Normal build" section.

First download `diffutils/gnulib`, which which is a dependency of `diffutils`. It is registered as a git submodule in _our_ repository. Run this from our repo root:

```bash
git submodule update --init --recursive --progress # clone diffutils/gnulib
```

Now **switch directory** with `cd diffutils`. All the following commands for "Initial setup" should be run in that directory.

Download and prepare various build files for diffutils:

```bash
./bootstrap --gnulib-srcdir=gnulib --no-git
```

You will see a few errors like `.git/hooks/applypatch-msg: No such file or directory` - that's expected since this script expects to be at the git repo root, which it isn't. You should see `./bootstrap: done. Now you can run './configure'.` which means that all the important stuff worked.

Now you need to run `configure` (another build preparation step):

```bash
./configure CFLAGS="-w"
```

The `CFLAGS` option ignores warnings during compilation. It is necessary depending your gcc version, so just include it.

### Normal build

This section assumes that you have already followed the "Initial setup" section. To (re-)build diffutils run `make` from the `diffutils` directory. You may want to run `cp src/diff $DIR/../diffutils.out` to put diffutils next to our other executables.

## Scripts

### Setup

The scripts work with Python 3.8.5 and pip 20.1.1. Similar versions should be fine. You can set up and activate a virtual environment with the pip dependencies like this:

```shell
cd scripts
python3 -m venv .venv
source .venv/bin/activate
pip install -r requirements.txt
```

If you need extra dependencies, please install them in the virtual environment, then run `pip freeze > requirements.txt` and commit `requirements.txt`.

### Generating random test cases

You can use [`gen_random_input.py`](./scripts/gen_random_input.py) to generate random test cases.

By default this script will write to [`test_cases/temp_random`](./test_cases/temp_random). This folder (and any others starting with `temp_`) are in `.gitignore`. They are useful for quick local testing. You can enable descriptive names and/or disable the temp prefix for more permanent test cases.

This script has no command line arguments. For quick experiments, just change the options in the file and don't commit that change. For scripting, import `generate_and_save_test_case` or `generate_input_pair` instead of running the script directly.

Here is roughly what the different options do:

- `strategy` - Either `independent` two totally random unrelated input files, or `add`, `remove` or `addremove` to generate the second input file by changing the first as specified.
- `length_1` - The length of the first input file. The length of the second input file depends on this and the other options.
- `change_strength` - 0 for no changes between both input files, 1 for many changes. Must be 1 for strategy `independent`.
- `chunkiness` - 0 for many independent single line changes, 1 roughly one large blocks of changes. Must be 0 for strategy `independent`.
- `distribution` - Distribution of the values in a single file. With `uniform` each number will appear once **on average**. With `zipf` numbers will follow [Zipf's law](https://en.wikipedia.org/wiki/Zipf%27s_law) (small numbers will appear much more often).

### Generating Size of Diff (Edit Distance)

The script [`diff_sizes.py`](./scripts/diff_sizes.py) will automatically go through all test cases and generate the diff size files for them.

```shell
cd scripts
python diff_sizes.py

Input Arguments
default:  generate missing diffs
all:      generate all diffs again
```

### Validation

The script [`validate.py`](./scripts/validate.py) goes through the test cases and applies the edit script `edit_script.txt`. After applying it to the first input file, it validates that the output matches the second input file. Next, it checks that the size of the diff matches what `diff --minimal` provides.

```shell
cd scripts
python validate.py

python validate.py test1 test2

Input Arguments
default:           apply the edit scripts and validate that the output matches
folder names..:    only validate the provided tests
```

### Test algorithm

The script [`test_algorithm.py`](./scripts/test_algorithm.py) combines functions from `gen_random_input.py` and `diff_sizes.py`. It runs our diff algorithm with random test cases and checks the results.

To run with default settings:

```shell
python -m scripts.test_algorithm
```

To get info about arguments pass `--help`.

### Run benchmarks

The script [`bench_algorithm.py`](./scripts/bench_algorithm.py) benchmarks our diff programs on generated inputs.

To run with default settings:

```shell
mpic++ src/main.cpp -O3 -DNDEBUG -o own-diff-mpi.out
g++ src/sequential.cpp -O3 -DNDEBUG -o own-diff-sequential.out
python -m scripts.bench_algorithm --output-csv temp-bench.csv
```

To get info about arguments pass `--help`.

## Output Formats for Tests

### Input Files

Labeled with `in_1.txt` and `in_2.txt`. They contain a number on every line. No empty line at the end.

### Edit Script

File `edit_script.txt` contains an editing script that transforms file `in_1.txt` into `in_2.txt`. No empty line at the end.

Each line start with the `index` (starting from 1) of the affected line in `in_1.txt` followed by the operation `+` or `-` for addition or deletion respectively. In the case of an addition, the third value is the content that will be newly inserted.

`+`Addition
The index refers to _after_ which line in `in_1.txt` the value will be inserted. If the insertion affects the very first line then the index should be 0.

`-`Deletion
The index of the line from `in_1.txt` that will be deleted.

**Example:**

```
in_1                    in_2                    Edit Script
		  +	1: 42			-> 0 + 42
		  +	2: 0			-> 0 + 0
		  +	3: 1			-> 0 + 1
		  +	4: 16			-> 0 + 16
		  +	5: 20			-> 0 + 20
 1: 0			7: 0
 2: 2			8: 2
 3: 31			9: 31
 4: 3			10: 3
		  +	11: 3			-> 4 + 3
 5: 20			12: 20
 6: 9			13: 9
 7: 2			14: 2
 8: 7			15: 7
 9: 7		  -				-> 9 -
10: 0		  -				-> 10 -
11: 2			16: 2
		  +	17: 33			-> 11 + 33
```

## Jupyter notebooks

To view and edit Jupyter notebooks, activate your Python virtual environment and run `jupyter-lab`. For information about setting up the virtual environment, see the "Scripts" section above.

## Useful commands

#### Calculate Size of Diff

Regular expression that matches any lines from the diff that begin with `<` or `>` and counts them.

```shell
diff --minimal in_1.txt in_2.txt | egrep -c "^[<>]"
```

#### Show the Diff visually

Presents the changes side-by-side in a easily readable format.

```shell
diff --minimal -y in_1.txt in_2.txt

a						a
b						b
c					  <
d						d
e						e
f						f
					  >	l
					  >	z
					  >	m
f						f
k						k
g						g
					  >	g
					  >	h
```

```shell
diff --minimal -u in_1.txt in_2.txt
@@ -1,9 +1,13 @@
 a
 b
-c
 d
 e
 f
+l
+z
+m
 f
 k
 g
+g
+h
```
