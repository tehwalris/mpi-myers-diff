from typing import Dict, Literal, Optional, Sequence
import numpy as np
from pathlib import Path
import math
import random


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


def random_interleaving(a: Sequence[int], b: Sequence[int]):
    p_take_a = len(a) / (len(a) + len(b))
    output = np.zeros(len(a) + len(b), dtype=int)
    ia, ib = 0, 0
    while ia < len(a) or ib < len(b):
        take_a = (not ia == len(a)) and (ib == len(b) or random.random() < p_take_a)
        if take_a:
            output[ia + ib] = a[ia]
            ia += 1
        else:
            output[ia + ib] = b[ib]
            ib += 1
    assert ia == len(a) and ib == len(b)
    return output


def generate_input_pair(
    strategy: Literal["independent", "remove", "add"],
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
    if strategy == "add":
        addition_count = int(math.floor(length_1 * change_strength))
        added_values = generate_input(
            addition_count, distribution, values_range=length_1
        )
        return (values_1, random_interleaving(values_1, added_values))
    else:
        raise ValueError(f"unsupported strategy {strategy}")


def test_case_name_from_config(config: Dict):
    parts = [
        "temp",
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
        "strategy": "add",
        "length_1": 30,
        "change_strength": 0.5,
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
