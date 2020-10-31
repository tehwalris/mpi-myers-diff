from typing import Iterable, Literal, Optional
import numpy as np
from pathlib import Path
from shutil import rmtree


def generate_input(
    length: int,
    distribution: Literal["uniform", "zipf"],
    values_range: Optional[int] = None,
):
    if distribution == "uniform":
        weights = np.ones(values_range)
    elif distribution == "zipf":
        weights = 1 / (np.arange(values_range) + 1)
    else:
        raise ValueError(f"unsupported distribution {distribution}")
    weights /= weights.sum()

    return np.random.choice(values_range, size=length, replace=True, p=weights)


def write_as_diff_input(values: Iterable[int], path: Path):
    with path.open("w", encoding="utf8") as f:
        for v in values:
            f.write(f"{v}\n")


if __name__ == "__main__":
    all_test_cases_dir = Path(__file__).parent / "../test_cases"

    test_case_dir = all_test_cases_dir / "temp"
    rmtree(test_case_dir, ignore_errors=True)
    test_case_dir.mkdir()

    values = generate_input(30, "zipf")
    write_as_diff_input(values, test_case_dir / "in_1.txt")
