import argparse
from pathlib import Path
import re
from statistics import geometric_mean
from typing import List
import sbatchman as sbm
import pandas as pd
import matplotlib.pyplot as plt
import numpy as np
import itertools

### === Constants and Plot Setup === ###
FONT_TITLE = 18
FONT_AXES = 18
FONT_TICKS = 16
FONT_LEGEND = 14
CHUNKSIZES = [16, 32, 64, 256, 1024, 4096]
OUT_DIR = Path("results")
OUT_DIR.mkdir(parents=True, exist_ok=True)
plt.style.use("seaborn-v0_8-colorblind")

plt.rc("axes", titlesize=FONT_AXES)
plt.rc("axes", labelsize=FONT_AXES)
plt.rc("xtick", labelsize=FONT_TICKS)
plt.rc("ytick", labelsize=FONT_TICKS)
plt.rc("legend", fontsize=FONT_LEGEND)
plt.rc("figure", titlesize=FONT_TITLE)


VALID_IMPLEMENTATIONS = ['atomic', 'mutex', 'openmp']
BEST_CHUNKSIZE = 64
BOARD_NAMES_MAP = {
  'baldo': 'AMD_EPYC_7742',
  'pioneer': 'Milk-V Pioneer',
  'bananaf3': 'Banana Pi F3',
  'arriesgado': 'HiFive Unmatched',
}

### === Functions === ###
def filter_jobs(jobs: List[sbm.Job]) -> List[sbm.Job]:
  return [job for job in jobs if job.status == "COMPLETED" and "compile" not in job.tag]


def parse_stdout(job: sbm.Job) -> float:
  stdout = job.get_stdout()
  times = []
  for line in stdout.splitlines():
    if line.startswith("run_id=") or line.startswith("run="):
      try:
        times.append(float(line.split(",")[-1]))
      except ValueError:
        continue
  return geometric_mean(times) if times else np.nan


def jobs_to_dataframe() -> pd.DataFrame:
  jobs = sbm.jobs_list(from_active=True, from_archived=True)
  jobs = filter_jobs(jobs)

  parsed_jobs = []
  for job in jobs:
    binary, args = job.parse_command_args()
    avg_runtime = parse_stdout(job)

    if job.tag.startswith("openmp"):
      chunksizes = CHUNKSIZES
      implementation = "openmp"
      dataset = Path(job.command.split()[1]).stem
    else:
      chunksizes = [int(re.match(r"\w+chunksize_(\d+)", job.tag).group(1))]
      implementation = re.match(r"(\w+)chunksize_\d+", job.tag).group(1)[:-1]
      dataset = Path(args["f"]).stem

    for chunksize in chunksizes:
      parsed_jobs.append({
        "dataset": dataset,
        "board": re.match(r"(\w+)_\d+cpus", job.config_name).group(1),
        "num_cpus": int(re.match(r"\w+_(\d+)cpus", job.config_name).group(1)),
        "chunksize": chunksize,
        "implementation": implementation,
        "runtime": avg_runtime,
      })

  df = pd.DataFrame(parsed_jobs)
  df.dropna(inplace=True)
  out_path = OUT_DIR / "data.csv"
  df.to_csv(out_path, index=False)
  print(f"CSV data saved to {out_path.resolve()}")
  return df


def load_csv_files(filepaths: List[str]) -> pd.DataFrame:
  dfs = []
  for file in filepaths:
    cluster = Path(file).stem.split("_")[0]
    df = pd.read_csv(file)
    df["cluster"] = cluster
    dfs.append(df)
  return pd.concat(dfs, ignore_index=True)


def line_plot(ax, data, title, color_map):
  for impl, grp in data.groupby("implementation"):
    mean_grp = grp.groupby("num_cpus", as_index=False)["runtime"].mean()
    ax.plot(
      mean_grp["num_cpus"],
      mean_grp["runtime"],
      marker="o",
      label=impl,
      color=color_map.get(impl),
    )
  ax.set_xlabel("Number of CPUs")
  ax.set_ylabel("Runtime [s]")
  ax.set_title(title)
  ax.grid(True)
  ax.legend(title="Implementation")


def generate_line_plots(df: pd.DataFrame):
  color_cycle = plt.rcParams["axes.prop_cycle"].by_key()["color"]
  implementations = df["implementation"].unique()
  color_map = dict(zip(implementations, itertools.cycle(color_cycle)))

  for board_name, df_board in df.groupby("board"):
    for dataset_name, df_dataset in df_board.groupby("dataset"):
      y_min, y_max = df_dataset["runtime"].min() * 0.95, df_dataset["runtime"].max() * 1.05
      x_ticks = sorted(df_dataset["num_cpus"].unique())
      chunks = sorted(df_dataset["chunksize"].unique())
      n_cols = int(len(chunks)**0.5 + 0.999)
      n_rows = (len(chunks) + n_cols - 1) // n_cols

      fig, axes = plt.subplots(n_rows, n_cols, figsize=(6 * n_cols, 5 * n_rows))
      axes = axes.flat if len(chunks) > 1 else [axes]

      for ax, ch in zip(axes, chunks):
        ax.set_ylim(y_min, y_max)
        ax.set_xticks(x_ticks)
        line_plot(ax, df_dataset[df_dataset["chunksize"] == ch], f"Chunksize = {ch}", color_map)

      for ax in axes[len(chunks):]:
        ax.axis("off")

      fig.suptitle(f"Strong Scaling - Graph: {dataset_name} - Board: {board_name}", y=0.98)
      fig.tight_layout()
      path = OUT_DIR / f"{board_name}_strong_scaling_{dataset_name}.png"
      plt.savefig(path)
      plt.close()
      print(f"Plot saved to {path.resolve()}")



def plot_board_comparisons(df: pd.DataFrame, min_coverage_ratio=0.5):
  df = df[df['chunksize'] == BEST_CHUNKSIZE]
  boards = df["board"].unique()
  datasets = df["dataset"].unique()
  implementations = sorted(df["implementation"].unique())

  # Assign unique colors to implementations
  cmap = plt.get_cmap("tab10")
  impl_colors = {impl: cmap(i) for i, impl in enumerate(implementations)}

  for dataset in datasets:
    df_dataset = df[df["dataset"] == dataset]

    for board1, board2 in itertools.combinations(boards, 2):
      fig, ax = plt.subplots(figsize=(12, 6))
      bar_width = 0.8 / len(implementations)

      # Build merged data for all implementations
      cpu_data = {}
      for impl in implementations:
        impl_df = df_dataset[df_dataset["implementation"] == impl]
        b1_df = impl_df[impl_df["board"] == board1]
        b2_df = impl_df[impl_df["board"] == board2]
        merged = pd.merge(
          b1_df, b2_df,
          on="num_cpus",
          suffixes=(f"_{board1}", f"_{board2}")
        )
        if not merged.empty:
          r1 = merged[f"runtime_{board1}"]
          r2 = merged[f"runtime_{board2}"]
          merged["norm_diff"] = (r1 - r2) / r1
          merged["speedup"] = np.where(r1 < r2, -r2 / r1, r1 / r2)
          
          for _, row in merged.iterrows():
            cpu = row["num_cpus"]
            if cpu not in cpu_data:
              cpu_data[cpu] = {}
            cpu_data[cpu][impl] = (row["norm_diff"], row["speedup"])

      # Filter CPU counts by coverage
      valid_cpu_counts = [
        cpu for cpu, impls in cpu_data.items()
        if len(impls) / len(implementations) >= min_coverage_ratio
      ]

      if not valid_cpu_counts:
        plt.close()
        continue

      valid_cpu_counts = sorted(valid_cpu_counts)
      x_base = np.arange(len(valid_cpu_counts))
      offsets = np.linspace(-0.4 + bar_width / 2, 0.4 - bar_width / 2, len(implementations))

      for i, impl in enumerate(implementations):
        norm_diffs = []
        speedups = []
        x_pos = []

        for idx, cpu in enumerate(valid_cpu_counts):
          if impl in cpu_data[cpu]:
            norm, spd = cpu_data[cpu][impl]
            norm_diffs.append(norm)
            speedups.append(spd)
            x_pos.append(x_base[idx] + offsets[i])

        if not norm_diffs:
          continue

        bars = ax.bar(
          x_pos,
          norm_diffs,
          width=bar_width,
          color=impl_colors[impl],
          label=impl
        )

        for bar, speedup in zip(bars, speedups):
          height = bar.get_height()
          offset = 0.04
          label_y = height + offset if height >= 0 else height - offset
          va = "bottom" if height >= 0 else "top"
          ax.text(bar.get_x() + bar.get_width() / 2, label_y, f"{speedup:.2f}x", ha="center", va=va, fontsize=11)

      ax.axhline(0, color="black", linewidth=1)
      ax.set_xticks(x_base)
      ax.set_xticklabels([str(cpu) for cpu in valid_cpu_counts])
      ax.set_xlabel("Number of CPUs")
      ax.set_ylabel(f"Performance Ratio") # \n(+ => {board2} faster, - => {board1} faster)")
      ax.set_title(f"{board1} vs {board2} - Dataset={dataset} - ChunkSize={BEST_CHUNKSIZE}")
      ax.legend(title="Implementation")
      ax.grid(True, axis="y", linestyle="--", alpha=0.6)
      fig.tight_layout()

      out_path = OUT_DIR / f"{dataset}_compare_{board1}_vs_{board2}_grouped.png"
      plt.savefig(out_path)
      plt.close()
      print(f"Grouped comparison plot saved to {out_path.resolve()}")


### === Main CLI === ###
def main():
  parser = argparse.ArgumentParser(description="Plot BFS benchmark results.")
  parser.add_argument("csv_files", nargs="*", help="CSV file(s) containing BFS results")
  args = parser.parse_args()

  if args.csv_files:
    df = load_csv_files(args.csv_files)
    print(f"Loaded data from {len(args.csv_files)} CSV file(s)")
  else:
    df = jobs_to_dataframe()

  # Filter
  df = df[df['implementation'].isin(VALID_IMPLEMENTATIONS)]
  # Remap board names
  df['board'] = df['board'].map(BOARD_NAMES_MAP)
  
  generate_line_plots(df)
  plot_board_comparisons(df)


if __name__ == "__main__":
  main()
