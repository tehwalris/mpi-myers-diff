# 2020 DPHPC Project - Longest Common Subsequence

## Building diffutils

### Automatic build

Run [`./scripts/diffutils_compile.sh`](./scripts/diffutils_compile.sh). The compiled `diff` binary is [`diffutils.out`](./diffutils.out) in our repo root.

If something goes wrong, try installing any missing tools and re-running the script. If it complains that aclocal is missing on the system then try the command `touch aclocal.m4 Makefile.in Makefile.am`.<br>
If that doesn't work, the explanations in "Manual build" might help you debug.

### Manual build

Here's a step-by-step guide for building diffutils. You probably want to use [`./scripts/diffutils_compile.sh`](./scripts/diffutils_compile.sh) instead of doing this manually.

The build for diffutils is a little different than what their official guide says, because diffutils is in a subdirectory inside our repo, not at the root.

#### Initial setup

You only have to perform these steps once. After that, you can just perform the steps in the "Normal build" section.

First download `diffutils/gnulib`, which which is a dependency of `diffutils`. It is registered as a git submodule in _our_ repository. Run this from our repo root:

```bash
git submodule update --init --recursive --progress
```

Now **switch directory**. All the following commands for "Initial setup" should be run in the `diffutils` directory.

```bash
cd diffutils
```

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

#### Normal build

This section assumes that you have already followed the "Initial setup" section. To (re-)build diffutils run `make` from the `diffutils` directory. You may want to run `cp src/diff ../diffutils.out` to put diffutils next to our other executables.

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
./scripts/compile_all.sh
./scripts/diffutils_compile.sh
python -m scripts.bench_algorithm prepare
python -m scripts.bench_algorithm run --output-csv temp-bench.csv
```

To get info about arguments pass `--help`.

The "prepare" phase generates random test cases. If you do "prepare" once and "run" multiple times, the same concrete inputs will be used for each run.

For running benchmarks on Euler, it's nice to run the benchmarks for different programs as separate batch jobs. That way the benchmark process is not wasting reserved cores by benchmarking a sequential program most of the time. Instead of using `bench_algorithm run`, you can use `bench_algorithm plan-batch`. It takes similar options, but instead of actually running anything, it gives you the commands to start the right batch jobs:

```shell
./scripts/compile_all.sh
./scripts/diffutils_compile.sh
python -m scripts.bench_algorithm prepare
python -m scripts.bench_algorithm plan-batch --output-dir temp-bench-out --limit-programs sequential_frontier,mpi_priority_frontier,mpi_no_master_frontier --mpi-procs 4 8 64
```

The `bench_algorithm plan-batch` command above will print the following:

```shell
mkdir -p temp-bench-out
bsub -n 4 'python -m scripts.bench_algorithm run --input-dir test_cases/temp_bench --limit-programs mpi_no_master_frontier --mpi-procs 4 --no-direct-mpi-procs-limit --output-csv temp-bench-out/mpi_no_master_frontier_4.csv'
bsub -n 8 'python -m scripts.bench_algorithm run --input-dir test_cases/temp_bench --limit-programs mpi_no_master_frontier --mpi-procs 8 --no-direct-mpi-procs-limit --output-csv temp-bench-out/mpi_no_master_frontier_8.csv'
bsub -n 64 'python -m scripts.bench_algorithm run --input-dir test_cases/temp_bench --limit-programs mpi_no_master_frontier --mpi-procs 64 --no-direct-mpi-procs-limit --output-csv temp-bench-out/mpi_no_master_frontier_64.csv'
bsub -n 4 'python -m scripts.bench_algorithm run --input-dir test_cases/temp_bench --limit-programs mpi_priority_frontier --mpi-procs 4 --no-direct-mpi-procs-limit --output-csv temp-bench-out/mpi_priority_frontier_4.csv'
bsub -n 8 'python -m scripts.bench_algorithm run --input-dir test_cases/temp_bench --limit-programs mpi_priority_frontier --mpi-procs 8 --no-direct-mpi-procs-limit --output-csv temp-bench-out/mpi_priority_frontier_8.csv'
bsub -n 64 'python -m scripts.bench_algorithm run --input-dir test_cases/temp_bench --limit-programs mpi_priority_frontier --mpi-procs 64 --no-direct-mpi-procs-limit --output-csv temp-bench-out/mpi_priority_frontier_64.csv'
bsub -n 1 'python -m scripts.bench_algorithm run --input-dir test_cases/temp_bench --limit-programs sequential_frontier --mpi-procs 1 --no-direct-mpi-procs-limit --output-csv temp-bench-out/sequential_frontier_1.csv'
```

Note that `plan-batch` it **does not run these commands**. You have to run them yourself to start the actual batch jobs.

## On Euler

It is required to first request an account to be granted access. In the following paragraph `username`refers to your nethz account.

connect with `ssh username@euler.ethz.ch`. You need to be in ETH VPN.

### Setup

```shell
git clone https://gitlab.ethz.ch/pascalm/2020-dphpc-project.git
```

Euler uses Modules that have to be loaded (compiler, libraries, python, etc.) first.

Execute the script [`setup_environment.sh`](./euler/setup_environment.sh) to load the required Modules. If the Python virtual environment is not setup already, the script will tell you to do so (see [script setup](#setup)), otherwise it is activated automatically.

```shell
source ./euler/setup_environment.sh
```

To compile all our binaries (optimized for Euler VI, which uses AMD Zen 2 processors) into the bin-euler folder, run the following command LOCALLY (Euler's GCC version does not support compiling for Zen 2). Then copy the bin-euler folder to the bin folder on Euler using rsync (see section below).

```shell
mkdir -p ./bin-euler
./scripts/compile_all_zen2.sh
```

### Running our benchmarks (WIP)

Prepare our benchmarks:
```shell
python -m scripts.bench_algorithm prepare --generation-strategies independent --min-file-size 30000 --max-file-size 100000 --target-file-size-steps 8 --num-regens=1 --output-dir ./test_cases/independent-small-benchmark/

python -m scripts.bench_algorithm prepare --generation-strategies add,remove,addremove --min-file-size 100000 --max-file-size 240000 --target-file-size-steps 6 --num-regens=1 --max-change-strength 0.5 --output-dir ./test_cases/add-remove-small-benchmark/

python -m scripts.bench_algorithm prepare --generation-strategies independent --min-file-size 100000 --max-file-size 2100000 --num-regens=1 --output-dir ./test_cases/independent-large-benchmark/

```

Create job commands for our benchmarks:

*If the job should run over night, you might want to add `--job-start-command-format "bsub -n %procs% -W 8:00 -R 'select[model==EPYC_7742]' -R 'rusage[mem=512]' %command%"` or something similar to prevent early termination due to a timeout*

**For the add-remove test case you need to add `-W 6:00` to the job commands with `-n 1`**
```shell
python -m scripts.bench_algorithm plan-batch --input-dir ./test_cases/independent-small-benchmark/ --output-dir ./benchmarks/independent-small-benchmark/ --limit-programs sequential_frontier,mpi_no_master_frontier --mpi-procs 1 2 3 4 8 16 32 64 --auto-repetitions

python -m scripts.bench_algorithm plan-batch --input-dir ./test_cases/independent-small-benchmark/ --output-dir ./benchmarks/independent-small-benchmark/ --limit-programs mpi_priority_frontier --mpi-procs 1 2 3 4 8 16 32 64 --auto-repetitions --max-repetitions 20


python -m scripts.bench_algorithm plan-batch --input-dir ./test_cases/add-remove-small-benchmark/ --output-dir ./benchmarks/add-remove-small-benchmark/ --limit-programs sequential_frontier,mpi_no_master_frontier --mpi-procs 1 2 3 4 8 16 32 64 --auto-repetitions

python -m scripts.bench_algorithm plan-batch --input-dir ./test_cases/add-remove-small-benchmark/ --output-dir ./benchmarks/add-remove-small-benchmark/ --limit-programs mpi_priority_frontier --mpi-procs 1 2 3 4 8 16 32 64 --auto-repetitions --max-repetitions 20


python -m scripts.bench_algorithm plan-batch --input-dir ./test_cases/independent-large-benchmark/ --output-dir ./benchmarks/independent-large-benchmark/ --limit-programs mpi_priority_frontier,mpi_no_master_frontier --mpi-procs 64 128 256 512 1024 --min-repetitions 1 --mpi-timeout-seconds 1200

```


---

How to copy files between PC and remote Euler:
(Note that `rsync -r` copies all the files from a folder which also includes generated files that would be in the `.gitignore` like `.venv` or `__pycache__`. You would need to exclude them all.)

```shell
# rsync [options] source destination
# -r for folder

# from remote:
rsync -r username@euler.ethz.ch:/cluster/home/username/dphpc/ local_dir/

# to remote:
rsync -r local_dir/ username@euler.ethz.ch:/cluster/home/username/dphpc/

# with excludes:
rsync -Pavr --exclude={.venv,__pycache__,.vscode} scripts/ username@euler.ethz.ch:/cluster/home/username/dphpc/scripts
```

If you need to load additional Modules use:

```shell
# upgrade to the new modulestack:
> source /cluster/apps/local/env2lmod.sh
> module available gcc

------------------------------------------------------------------------------------------- /cluster/apps/lmodules/Core -------------------------------------------------------------------------------------------
   gcc/4.8.2    gcc/4.8.5    gcc/5.4.0    gcc/6.3.0    gcc/7.3.0    gcc/8.2.0 (L,D)

  Where:
   L:  Module is loaded
   D:  Default Module

Use "module spider" to find all possible modules.
Use "module keyword key1 key2 ..." to search for all possible modules matching any of the "keys".




> module load gcc/8.2.0
```

### Submit Jobs

First, test out that the code works by executing it directly on the login node.

**You probably want to use `bench_algorithm plan-batch`** instead of manually writing these commands. See "Scripts -> Run benchmarks" above.

Submit job with:

```shell
# bsub -n CORES -W (HH:MM or MINUTES. Default is 4h) command args
# make sure that -n and our --mpi-procs match.
bsub -n 32 -W 4:00 python -m scripts.bench_algorithm --output-csv=/cluster/home/username/dphpc/benchmarks/test.csv --mpi-procs=32 --limit-programs=mpi_no_master,sequential_priority

# Check the status of the job:
bbjobs
bbjobs JOBID

# show all pending jobs:
bjobs

# Check the output of the currently running job:
# Useful to see the progress bar and estimate remaining time.
bpeek

# kill submitted job
bkill
bkill JOBID
```

The output and some additional information of the job can be found in the working directory where you submitted it in a file called `lsf.oJOBID`. And the csv should be stored in the path that you specified.

For more information check out the [Euler Wiki](https://scicomp.ethz.ch/wiki/Getting_started_with_clusters).

## Output Formats for Tests

### Input Files

Labeled with `in_1.txt` and `in_2.txt`. They contain a number on every line. No empty line at the end.

### Edit Script

File `edit_script.txt` contains an editing script that transforms file `in_1.txt` into `in_2.txt`. No empty line at the end.

Each line start with the `index` (starting from 1) of the affected line in `in_1.txt` followed by the operation `+` or `-` for addition or deletion respectively. In the case of an addition, the third value is the content that will be newly inserted.

**`+`Addition**

The index refers to _after_ which line in `in_1.txt` the value will be inserted. If the insertion affects the very first line then the index should be 0.

**`-`Deletion**

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

To view and edit Jupyter notebooks, activate your Python virtual environment and run `jupyter-lab`. For information about setting up the virtual environment, see the [Scripts section](#setup) above.

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
