from .gen_random_input import generate_and_save_test_case, test_case_name_from_config
from .run_algorithm import run_sequential_measure_snakes
import argparse
import csv
from pathlib import Path

test_case_folder = Path(__file__).parent / "../test_cases"

parser = argparse.ArgumentParser(
    description="Measure the number of comparisons to compute the snakes per DP entry"
)
parser.add_argument(
    "--output-folder",
    type=str,
    required=True,
    help="path where to write the results as CSV files",
)
parser.add_argument(
    "--test-case",
    type=str,
    help="name of a testcase folder to measure",
)

generation_configs = [
    {
        "strategy": "add",
        "length_1": 100,
        "change_strength": 0.3,
        "chunkiness": 0,
        "distribution": "zipf",
    },
    {
        "strategy": "add",
        "length_1": 100,
        "change_strength": 0.3,
        "chunkiness": 1,
        "distribution": "zipf",
    },
    {
        "strategy": "add",
        "length_1": 300,
        "change_strength": 0.1,
        "chunkiness": 0,
        "distribution": "zipf",
    },
    {
        "strategy": "remove",
        "length_1": 100,
        "change_strength": 0.3,
        "chunkiness": 0,
        "distribution": "zipf",
    },
    {
        "strategy": "remove",
        "length_1": 100,
        "change_strength": 0.3,
        "chunkiness": 1,
        "distribution": "zipf",
    },
    {
        "strategy": "remove",
        "length_1": 300,
        "change_strength": 0.1,
        "chunkiness": 0,
        "distribution": "zipf",
    },
    {
        "strategy": "addremove",
        "length_1": 100,
        "change_strength": 0.3,
        "chunkiness": 0,
        "distribution": "zipf",
    },
    {
        "strategy": "addremove",
        "length_1": 100,
        "change_strength": 0.3,
        "chunkiness": 1,
        "distribution": "zipf",
    },
    {
        "strategy": "addremove",
        "length_1": 150,
        "change_strength": 0.1,
        "chunkiness": 0,
        "distribution": "zipf",
    },
    {
        "strategy": "independent",
        "length_1": 20,
        "change_strength": 1,
        "chunkiness": 0,
        "distribution": "zipf",
    },
    {
        "strategy": "independent",
        "length_1": 25,
        "change_strength": 1,
        "chunkiness": 0,
        "distribution": "uniform",
    },
    {
        "strategy": "independent",
        "length_1": 30,
        "change_strength": 1,
        "chunkiness": 0,
        "distribution": "zipf",
    },
]


def measure_test_case(test_case_name: str, csv_folder: Path):
    csv_output_file = open(csv_folder / (test_case_name + ".csv"), "w", newline="")
    csv_writer = csv.writer(csv_output_file)

    csv_writer.writerow(["d", "k", "comparisons"])

    test_case_dir = test_case_folder / test_case_name
    output = run_sequential_measure_snakes(
        test_case_dir / "in_1.txt", test_case_dir / "in_2.txt"
    )

    for t in output.tuples:
        csv_writer.writerow(t)

    csv_output_file.close()


if __name__ == "__main__":
    args = parser.parse_args()
    csv_folder = Path(args.output_folder)

    if args.test_case:  # measure single test case
        measure_test_case(args.test_case, csv_folder)
    else:  # measure all test cases given in generation_configs
        for generation_config in generation_configs:
            test_case_name = test_case_name_from_config(
                generation_config, temporary=False, detailed_name=True
            )
            test_case_dir = test_case_folder / test_case_name
            if not test_case_dir.exists():  # don't regenerate
                generate_and_save_test_case(
                    generation_config, temporary=False, detailed_name=True
                )

            measure_test_case(test_case_name, csv_folder)
