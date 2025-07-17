from pathlib import Path
import re
from statistics import geometric_mean
from typing import List
import sbatchman as sbm
from pprint import pprint
import pandas as pd
import matplotlib.pyplot as plt
import numpy as np
import itertools

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

### Jobs Functions ###

CHUNKSIZES = [16, 32, 64, 256, 1024, 4096]

def filter_jobs(jobs: List[sbm.Job]) -> List[sbm.Job]:
  filtered_jobs = []
  for job in jobs:
    if job.status in ['COMPLETED'] and 'compile' not in job.tag:
      filtered_jobs.append(job)
  return filtered_jobs

def parse_stdout(job: sbm.Job) -> float:
  stdout = job.get_stdout()
  try:
    lines = stdout.split('\n')
    times = []
    for line in lines:
      if line.startswith('run_id=') or line.startswith('run='):
        times.append(float(line.split(',')[-1]))
  except Exception as e:
    print(e)
    print(f'STDOUT:\n{stdout}')
    exit(1)

  # TODO remove times for which diameter too high/low
  
  return geometric_mean(times) if len(times) > 0 else np.nan

### END Jobs Functions ###


jobs = sbm.jobs_list(from_active=True, from_archived=True)
jobs = filter_jobs(jobs)


### Jobs to Dataframe ###

parsed_jobs = []
for job in jobs:
  binary, args = job.parse_command_args()
  avg_runtime = parse_stdout(job)

  if job.tag.startswith('openmp'):
    chunksizes = CHUNKSIZES
    implementation = 'openmp'
    dataset = Path(job.command.split(' ')[1]).stem
    if dataset == 'europe_osm':
      print(job.get_stdout())
  else:
    chunksizes = [int(re.match(r'\w+chunksize_(\d+)', job.tag).group(1))]
    implementation = re.match(r'(\w+)chunksize_\d+', job.tag).group(1)[:-1]
    dataset = Path(args['f']).stem

  for chunksize in chunksizes:
    parsed_jobs.append({
      'dataset': dataset,
      'board': re.match(r'(\w+)_\d+cpus', job.config_name).group(1),
      'num_cpus': int(re.match(r'\w+_(\d+)cpus', job.config_name).group(1)),
      'chunksize': chunksize,
      'implementation': implementation,
      'runtime': avg_runtime,
    })

df = pd.DataFrame(parsed_jobs)
df.dropna(inplace=True)
df.to_csv(OUT_DIR / 'data.csv')
print(f'CSV data saved to {(OUT_DIR / "data.csv").resolve().absolute()}')
print(df)

# Assign a unique color to each implementation

color_cycle = plt.rcParams['axes.prop_cycle'].by_key()['color']
implementations = df['implementation'].unique()
implementation_color_map = dict(zip(implementations, itertools.cycle(color_cycle)))
# df['color'] = df['implementation'].map(color_map)

### END Jobs to Dataframe ###

### Plots ###
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
      color=implementation_color_map[impl],
    )
  ax.set_xlabel("Number of CPUs")
  ax.set_ylabel("Runtime [s]")
  ax.set_title(title)
  ax.grid(True)
  ax.legend(title="Implementation")

for board_name, df_board in df.groupby('board'):
  for dataset_name, df_dataset in df_board.groupby('dataset'):

    y_lim_top = df_dataset['runtime'].max() * 1.05
    y_lim_bottom = df_dataset['runtime'].min() * 0.95
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

### END Plots ###
