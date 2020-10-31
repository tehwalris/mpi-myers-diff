from typing import Dict, Iterable, Literal, Optional
import numpy as np
from pathlib import Path
from shutil import rmtree


def generate_input(
    length: int,
    distribution: Literal["uniform", "zipf"],
    values_range: Optional[int] = None,
):
    if values_range is None:
        values_range = length

    if distribution == "uniform":
        weights = np.ones(values_range)
    elif distribution == "zipf":
        weights = 1 / (np.arange(values_range) + 1)
    else:
        raise ValueError(f"unsupported distribution {distribution}")
    weights /= weights.sum()

    return np.random.choice(values_range, size=length, replace=True, p=weights)


def generate_input_pair(
    strategy: Literal["independent", "remove"],
    length_1: int,
    change_strength: float,  # 0 - no changes, 1 - many changes
    distribution: Literal["uniform", "zipf"],
):
    if strategy == "independent":
        assert change_strength == 1
        return tuple(generate_input(length_1, distribution) for _ in range(2))

    values_1 = generate_input(length_1, distribution)
    assert len(values_1) == length_1

    if strategy == "remove":
        return (values_1, values_1[np.random.rand(length_1) > change_strength])


def test_case_name_from_config(config: Dict):
    parts = [
        "random",
        config["strategy"],
        config["length_1"],
        format(config["change_strength"], ".2f"),
        config["distribution"],
    ]
    return "_".join(str(p) for p in parts)


if __name__ == "__main__":
    all_test_cases_dir = Path(__file__).parent / "../test_cases"

    config = {
        "strategy": "remove",
        "length_1": 30,
        "change_strength": 0.8,
        "distribution": "zipf",
    }

    test_case_dir = all_test_cases_dir / test_case_name_from_config(config)
    test_case_dir.mkdir(exist_ok=True)
    for path in test_case_dir.glob("in_*.txt"):
        path.unlink()

    for i, values in enumerate(generate_input_pair(**config)):
        with (test_case_dir / f"in_{i + 1}.txt").open("w", encoding="utf8") as f:
            for v in values:
                f.write(f"{v}\n")
