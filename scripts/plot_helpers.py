import seaborn as sns

nice_name_map = {
    "input_length_1": "Input size (file 1) [elements]",
    "nd": "Expected number of calculated cells (n * d)",
    "seconds_per_cell": "Calculation time per cell [s]",
    "seconds_until_len": "Calculation time (only edit distance) [s]",
    "time_relative_to_sequential": "Time relative to sequential times cores",
    "mpi_procs": "Cores",
    "mpi_priority_frontier": "MPI dynamic priority",
    "mpi_no_master_frontier": "MPI row-wise",
    "sequential_frontier": "Sequential",
}


def plot_scatter_with_lines(
    ax,
    scatter_data,
    line_data,
    x_key,
    y_key,
    hue_key,
    hue_to_label=lambda v: str(int(v)),
    show_legend=True,
):
    def get_palette(name=None):
        return {
            mpi_procs: color
            for mpi_procs, color in zip(
                sorted(scatter_data[hue_key].dropna().unique()),
                sns.color_palette(name),
            )
        }

    line_data.plot(ax=ax, legend=False, color=get_palette("pastel"), zorder=0)
    sns.scatterplot(
        data=scatter_data,
        x=x_key,
        y=y_key,
        hue=hue_key,
        palette=get_palette(),
        ax=ax,
        legend=show_legend,
    )
    if show_legend:
        labels = [hue_to_label(v) for v in sorted(scatter_data[hue_key].unique())]
        handles_to_show = ax.legend().legendHandles[len(labels) : 2 * len(labels)]
        ax.legend(
            handles=handles_to_show,
            labels=labels,
            title=nice_name_map[hue_key],
            ncol=len(labels) // 2,
        )
    ax.set_xlabel(nice_name_map[x_key])
    ax.set_ylabel(nice_name_map[y_key])