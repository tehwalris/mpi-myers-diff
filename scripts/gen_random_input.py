from typing import Dict, Literal, Optional, Sequence
import numpy as np
from pathlib import Path
import math
import random


def generate_input_1(
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


def random_positive_integers_to_sum(sum: int, count: int):
    assert 1 <= count <= sum
    cuts = np.random.choice(sum, size=count, replace=False)
    cuts = np.append(cuts, [sum])
    cuts.sort()
    cuts[0] = 0
    assert len(np.unique(cuts)) == len(cuts)
    values = cuts[1:] - cuts[:-1]
    assert (values > 0).all()
    assert values.sum() == sum
    return values


def random_chunky_mask(size: int, p_true: float, chunkiness: float):
    assert 0 <= chunkiness <= 1
    # HACK This is an attempt at making the chunkiness of the result less dependent on p_true.
    chunkiness = min(
        chunkiness, math.sqrt(chunkiness ** 2 + p_true ** 2) / math.sqrt(2)
    )
    assert 0 <= chunkiness <= 1

    base_mask = np.random.rand(size) < p_true
    chunk_count = int(math.floor(base_mask.sum() ** (1 - chunkiness)))
    print("DEBUG", chunk_count)
    chunk_sizes = random_positive_integers_to_sum(base_mask.sum(), chunk_count)

    chunk_ranges = []
    cur_size, cur_start, cur_chunk = 0, 0, 0
    for i in range(size):
        assert cur_start + cur_size <= i
        if cur_chunk == chunk_count:
            break
        if not base_mask[i]:
            if cur_size == 0:
                cur_start = i + 1
            continue
        cur_size += 1
        if cur_size == chunk_sizes[cur_chunk]:
            chunk_ranges.append((cur_start, i + 1))
            cur_chunk += 1
            cur_size = 0
            cur_start = i + 1
    assert len(chunk_ranges) == chunk_count

    chunky_mask = np.zeros(size, dtype=bool)
    for (min_start, max_end), chunk_size in zip(chunk_ranges, chunk_sizes):
        wiggle_room = (max_end - min_start) - chunk_size
        assert wiggle_room >= 0
        start = min_start + random.randint(0, wiggle_room)
        end = start + chunk_size
        chunky_mask[start:end] = True
    assert base_mask.sum() == chunky_mask.sum()
    return chunky_mask


def generate_input_pair(
    length_1: int,
    strategy: Literal["independent", "remove", "add", "addremove"],
    change_strength: float,  # 0 - no changes, 1 - many changes
    chunkiness: float,  # 0 - individual random changes, 1 - all changes consecutive
    distribution: Literal["uniform", "zipf"],
):
    assert 0 < change_strength <= 1

    if strategy == "independent":
        assert change_strength == 1
        assert chunkiness == 0
        return tuple(generate_input_1(length_1, distribution) for _ in range(2))

    def generate_input_2(values_1, strategy, change_strength):
        # important, since values_1 may be different than in outer scope
        length_1 = len(values_1)

        if strategy == "remove":
            return values_1[~random_chunky_mask(length_1, change_strength, chunkiness)]
        elif strategy == "add":
            assert chunkiness == 0
            addition_count = int(math.floor(length_1 * change_strength))
            added_values = generate_input_1(
                addition_count, distribution, values_range=length_1
            )
            return random_interleaving(values_1, added_values)
        elif strategy == "addremove":
            values_with_added = generate_input_2(values_1, "add", change_strength)
            # roughly keep original length
            remove_change_strength = 1 - (1 / (1 + change_strength))
            return generate_input_2(values_with_added, "remove", remove_change_strength)
        else:
            raise ValueError(f"unsupported strategy {strategy}")

    values_1 = generate_input_1(length_1, distribution)
    assert len(values_1) == length_1
    return (values_1, generate_input_2(values_1, strategy, change_strength))


def test_case_name_from_config(config: Dict):
    parts = [
        "temp",
        "random",
        # config["strategy"],
        # config["length_1"],
        # format(config["change_strength"], ".2f"),
        # format(config["chunkiness"], ".2f"),
        # config["distribution"],
    ]
    return "_".join(str(p) for p in parts)


def generate_and_save_test_case(config: Dict):
    all_test_cases_dir = Path(__file__).parent / "../test_cases"

    test_case_dir = all_test_cases_dir / test_case_name_from_config(config)
    test_case_dir.mkdir(exist_ok=True)
    for path in test_case_dir.glob("in_*.txt"):
        path.unlink()

    for i, values in enumerate(generate_input_pair(**config)):
        with (test_case_dir / f"in_{i + 1}.txt").open("w", encoding="utf8") as f:
            for v in values:
                f.write(f"{v}\n")


if __name__ == "__main__":
    generate_and_save_test_case(
        {
            "strategy": "remove",
            "length_1": 70,
            "change_strength": 0.4,
            "chunkiness": 0.5,
            "distribution": "zipf",
        }
    )
