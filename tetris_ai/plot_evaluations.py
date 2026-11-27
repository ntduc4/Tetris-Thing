"""Render all saved encoding histories and the matched v5 comparison to imgs/.

Run from anywhere with: python tetris_ai/plot_evaluations.py
Requires matplotlib (plotting only; training does not need it).
"""

import json
from pathlib import Path

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib.ticker import PercentFormatter


ROOT = Path(__file__).resolve().parent.parent
LOGS = ROOT / "tetris_ai" / "checkpoints" / "encoding_v5" / "shared"
OUTPUT = ROOT / "imgs"
HISTORIES = {
    "v3": (
        ("Recorded history (mostly Monte Carlo)", "evaluations.jsonl", "#327caa"),
    ),
    "v4": (
        ("Current history", "evaluations.jsonl", "#327caa"),
        ("Archived history", "old_evaluations.jsonl", "#da6745"),
        ("Archived MC → Q", "old_monte_carlo_evaluations.jsonl", "#8c64a3"),
        ("Archived 35-step Q", "old_35-step_qlearn_evaluations.jsonl", "#71a354"),
    ),
    "v5": (
        ("Q-learning", "evaluations.jsonl", "#da6745"),
        ("Monte Carlo", "49k_evals_monte_carlo.jsonl", "#327caa"),
    ),
}
METRICS = (
    ("mean_score", "Mean score", None),
    ("mean_pieces_placed", "Mean pieces placed (max 500)", None),
    ("attack_per_piece", "Attack per piece", None),
    ("piece_cap_rate", "500-piece cap rate", "percent"),
)


def load_runs(version):
    runs = []
    for label, filename, color in HISTORIES[version]:
        with (LOGS.parent.parent / f"encoding_{version}" / "shared" / filename).open(
            encoding="utf-8"
        ) as file:
            # Use the latest entry when a checkpoint has been evaluated twice.
            rows = {}
            for line in file:
                if line.strip():
                    row = json.loads(line)
                    rows[row["episodes_trained"]] = row
        runs.append((label, color, [rows[episode] for episode in sorted(rows)]))

    for _, _, rows in runs:
        assert all(
            row["seeds"] == [987654321, 987654322, 987654323]
            and row["piece_cap"] == 500
            for row in rows
        ), "Evaluation settings differ from the documented benchmark"
    if version == "v5":
        assert [row["episodes_trained"] for row in runs[0][2]] == [
            row["episodes_trained"] for row in runs[1][2]
        ], "v5 evaluation episodes do not align"
    return runs


def rolling(values, count=10):
    return [
        sum(values[max(0, i - count + 1): i + 1])
        / min(i + 1, count)
        for i in range(len(values))
    ]


def trend_chart(version, runs):
    fig, axes = plt.subplots(2, 2, figsize=(12, 7), sharex=True)
    for ax, (key, title, tick_format) in zip(axes.flat, METRICS):
        for label, color, rows in runs:
            x = [row["episodes_trained"] / 1000 for row in rows]
            y = [row[key] for row in rows]
            ax.plot(x, y, color=color, alpha=0.16, linewidth=0.7)
            ax.plot(x, rolling(y), color=color, linewidth=2, label=label)
        ax.set_title(title)
        ax.grid(alpha=0.2)
        if tick_format == "percent":
            ax.set_ylim(0, 1.03)
            ax.yaxis.set_major_formatter(PercentFormatter(1))
        elif key == "mean_pieces_placed":
            ax.set_ylim(0, 520)
    for ax in axes[1]:
        ax.set_xlabel("Training episodes (thousands)")
    fig.legend(*axes[0, 0].get_legend_handles_labels(), loc="upper center",
               ncol=min(len(runs), 2), bbox_to_anchor=(0.5, 0.98))
    fig.suptitle(f"{version} · fixed-seed greedy evaluation (3 games, 500-piece cap)",
                 y=1.04, fontsize=14)
    fig.text(0.5, 0.005, "Faint: each 100-episode evaluation · Solid: trailing 10 evaluations",
             ha="center", fontsize=9)
    fig.tight_layout(rect=(0, 0.025, 1, 0.88 if len(runs) > 2 else 0.94))
    fig.savefig(OUTPUT / f"{version}-evaluation-trends.png", bbox_inches="tight")
    plt.close(fig)


def window_chart(runs):
    # Seven equal-sized blocks: 70 evaluations (7,000 episodes) per block.
    windows = tuple((low, low + 7000) for low in range(0, 49000, 7000))
    labels = [
        f"{low / 1000 + 0.1:g}–{high / 1000:g}k"
        for low, high in windows
    ]
    fig, axes = plt.subplots(1, 3, figsize=(15, 4.2))
    for ax, (key, title, tick_format) in zip(
        axes, (METRICS[0], METRICS[1], METRICS[3])
    ):
        for offset, (label, color, rows) in zip((-0.2, 0.2), runs):
            means = [
                sum(row[key] for row in rows if low < row["episodes_trained"] <= high)
                / sum(low < row["episodes_trained"] <= high for row in rows)
                for low, high in windows
            ]
            ax.bar([i + offset for i in range(len(windows))], means, 0.38,
                   label=label, color=color)
        ax.set_title(title)
        ax.set_xticks(range(len(windows)), labels, rotation=35, ha="right")
        ax.tick_params(axis="x", labelsize=8)
        ax.grid(axis="y", alpha=0.2)
        ax.set_axisbelow(True)
        if tick_format == "percent":
            ax.set_ylim(0, 1)
            ax.yaxis.set_major_formatter(PercentFormatter(1))
    fig.legend(*axes[0].get_legend_handles_labels(), loc="upper center",
               ncol=2, bbox_to_anchor=(0.5, 1.02))
    fig.suptitle("v5 matched 7,000-episode windows · 70 evaluations per bar",
                 y=1.12)
    fig.tight_layout()
    fig.savefig(OUTPUT / "v5-window-comparison.png", bbox_inches="tight")
    plt.close(fig)


def main():
    OUTPUT.mkdir(exist_ok=True)
    plt.rcParams.update({"font.size": 10, "axes.spines.top": False,
                         "axes.spines.right": False, "savefig.dpi": 160})
    for version in HISTORIES:
        runs = load_runs(version)
        trend_chart(version, runs)
        if version == "v5":
            window_chart(runs)
    print(f"Wrote charts to {OUTPUT}")


if __name__ == "__main__":
    main()
