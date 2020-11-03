# 2020 DPHPC Project - Longest Common Subsequence

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

The script `diff_sizes.py` will automatically go through all test cases and generate the diff size files for them.

```shell
cd scripts
python diff_sizes.py

Input Arguments
default:  generate missing diffs
all:      generate all diffs again
```

### Validation

The script `validate.py` goes through the test cases and applies the edit script `edit_script.txt`. After applying it to the first input file, it validates that the output matches the second input file. Next, it checks that the size of the diff matches what `diff --minimal` provides.

```shell
cd scripts
python validate.py

python validate.py test1 test2

Input Arguments
default:           apply the edit scripts and validate that the output matches
folder names..:    only validate the provided tests
```

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

