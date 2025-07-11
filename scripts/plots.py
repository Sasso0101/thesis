from pathlib import Path
import re
from statistics import geometric_mean
import sbatchman as sbm
from pprint import pprint
import pandas as pd
import matplotlib.pyplot as plt
import numpy as np

FONT_TITLE = 18
FONT_AXES = 18
FONT_TICKS = 16
FONT_LEGEND = 14

plt.rc('axes', titlesize=FONT_AXES)       # fontsize of the axes title
plt.rc('axes', labelsize=FONT_AXES)       # fontsize of the x and y labels
plt.rc('xtick', labelsize=FONT_TICKS)     # fontsize of the tick labels
plt.rc('ytick', labelsize=FONT_TICKS)     # fontsize of the tick labels
plt.rc('legend', fontsize=FONT_LEGEND)    # legend fontsize
plt.rc('figure', titlesize=FONT_TITLE)    # fontsize of the figure title

OUT_DIR = Path('results')
OUT_DIR.mkdir(parents=True, exist_ok=True)

jobs = sbm.jobs_list()
# pprint(jobs)

def parse_stdout(stdout: str) -> float:
  lines = stdout.split('\n')
  times = []
  for i in range(2, len(lines), 2):
    parts = lines[i].split(',')
    if len(parts) > 3 and int(parts[1]) > 3:
      times.append(float(parts[-1]))
  return geometric_mean(times) if len(times) > 0 else np.nan

parsed_jobs = []
for job in jobs:
  if job.status not in ['COMPLETED', 'TIMEOUT'] or job.tag == 'compile':
    # print(('-'*40)+'\nSkipping')
    # pprint(job)
    continue

  binary, args = job.parse_command_args()
  avg_runtime = parse_stdout(job.get_stdout())

  if not args or 'f' not in args:
    print(('-'*40)+'\nWeird')
    print(binary)
    print(args)
    pprint(job)
    continue

  parsed_jobs.append({
    'dataset': Path(args['f']).stem,
    'board': re.match(r'(\w+)_\d+cpus', job.config_name).group(1),
    'num_cpus': int(re.match(r'\w+_(\d+)cpus', job.config_name).group(1)),
    'chunksize': int(re.match(r'\w+chunksize_(\d+)', job.tag).group(1)),
    'implementation': re.match(r'(\w+)chunksize_\d+', job.tag).group(1)[:-1],
    'runtime': avg_runtime,
  })

df = pd.DataFrame(parsed_jobs)
print(df)

df.dropna(inplace=True)

# Plotting style
plt.style.use("seaborn-v0_8-colorblind")

def line_plot(ax, data, title):
  """Plot num_cpus vs runtime, one line per implementation on ax."""
  for impl, grp in data.groupby("implementation"):
    # average runs with same (impl, num_cpus)
    mean_grp = grp.groupby("num_cpus", as_index=False)["runtime"].mean()
    ax.plot(
      mean_grp["num_cpus"],
      mean_grp["runtime"],
      marker="o",
      label=impl,
    )
  ax.set_xlabel("Number of CPUs")
  ax.set_ylabel("Runtime [s]")
  ax.set_title(title)
  ax.grid(True)
  ax.legend(title="Implementation")


for board_name, df_board in df.groupby('board'):
  for dataset_name, df_dataset in df_board.groupby('dataset'):

    y_lim_top = df_dataset['runtime'].max() * 1.2
    y_lim_bottom = df_dataset['runtime'].min() * 0.8
    x_ticks = df_dataset['num_cpus'].unique().astype(int)

    chunks = sorted(df_dataset["chunksize"].unique())
    n = len(chunks)
    # choose grid size ~square
    n_cols = int(n**0.5 + 0.999)
    n_rows = (n + n_cols - 1) // n_cols

    fig, axes = plt.subplots(n_rows, n_cols, figsize=(6 * n_cols, 5 * n_rows))
    axes = axes.flat if n > 1 else [axes]

    for ax, ch in zip(axes, chunks):
      ax.set_ylim(y_lim_bottom, y_lim_top)
      ax.set_xticks(x_ticks)
      ch_grp = df_dataset[df_dataset["chunksize"] == ch]
      line_plot(ax, ch_grp, f"Chunksize = {ch}")

    # Hide unused axes (if any)
    for ax in axes[n:]:
      ax.axis("off")

    fig.suptitle(f"Strong Scaling - Graph: {dataset_name} - Board: {board_name}", y=0.98)
    fig.tight_layout()
    path = OUT_DIR / f'{board_name}_strong_scaling_{dataset_name}.png'
    plt.savefig(path)
    print(f'Plot saved to {path.resolve().absolute()}')
    plt.close()


  # plt.figure(figsize=(8, 6))
  # for impl_name, impl_group in group.groupby('implementation'):
  #   avg_group = impl_group.groupby('num_cpus')['runtime'].mean().reset_index()
  #   plt.plot(avg_group['num_cpus'], avg_group['runtime'], marker='o', label=impl_name)

  # plt.title(f"Runtime vs CPUs — Dataset: {dataset_name}")
  # plt.xlabel("Number of CPUs")
  # plt.ylabel("Runtime (s)")
  # plt.legend(title="Implementation")
  # plt.grid(True)
  # plt.tight_layout()
  # path = OUT_DIR / f'strong_scaling_{dataset_name}.png'
  # plt.savefig(path)  # Save per-dataset plot
  # print(f'Plot saved to {path.resolve().absolute()}')
  # plt.close()

# === Summary Plot (average over datasets and chunksizes) ===
# plt.figure(figsize=(8, 6))
# for impl_name, impl_group in df.groupby('implementation'):
#     avg_group = impl_group.groupby('num_cpus')['runtime'].mean().reset_index()
#     plt.plot(avg_group['num_cpus'], avg_group['runtime'],
#              marker='o', label=impl_name)

# plt.title("Average Runtime vs CPUs (All Datasets)")
# plt.xlabel("Number of CPUs")
# plt.ylabel("Average Runtime (s)")
# plt.legend(title="Implementation")
# plt.grid(True)
# plt.tight_layout()
# path = OUT_DIR / 'strong_scaling_summary.png'
# plt.savefig(path)
# print(f'Plot saved to {path.resolve().absolute()}')
# plt.close()
