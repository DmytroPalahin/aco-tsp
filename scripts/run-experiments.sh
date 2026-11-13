#!/usr/bin/env bash
#
# Regenerates every data file behind the figures and tables of the report.
#
#   scripts/run-experiments.sh <path to aco-tsp> [output directory]
#
# The output directory defaults to report/data. Everything is seeded, so the
# tour lengths are reproducible; only the timings of the scaling experiment
# depend on the machine. Use a Release build: a full run then takes a few
# minutes.

set -euo pipefail

if [[ $# -lt 1 || $# -gt 2 ]]; then
    echo "usage: $0 <path to aco-tsp> [output directory]" >&2
    exit 2
fi

ACO_TSP=$1
ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
OUT=${2:-$ROOT/report/data}
COURSE=$ROOT/data/course
RANDOM_DIR=$ROOT/data/random

RUNS=20         # independent seeds per configuration
ITERATIONS=200  # the solver's default budget, spelled out for the record

UNIFORM=$RANDOM_DIR/random100.tsp # the instance used for tuning and plots

if ! "$ACO_TSP" --version >/dev/null 2>&1; then
    echo "$0: cannot run '$ACO_TSP'" >&2
    exit 2
fi
if [[ ! -f $UNIFORM ]]; then
    echo "$0: $UNIFORM is missing" >&2
    exit 2
fi

mkdir -p "$OUT"
WORK=$(mktemp -d)
trap 'rm -rf "$WORK"' EXIT

# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

# solve <instance> [options...]: prints the length of the tour found.
solve() {
    "$ACO_TSP" solve "$@" --quiet
}

# repeat <instance> [options...]: one tour length per line, for seeds 1..RUNS.
repeat() {
    local seed
    for ((seed = 1; seed <= RUNS; seed++)); do
        solve "$@" --seed "$seed"
    done
}

# Reads numbers on standard input and prints "mean std min max".
stats() {
    awk '
        NR == 1 { min = $1; max = $1 }
        { sum += $1; squares += $1 * $1
          if ($1 < min) min = $1
          if ($1 > max) max = $1 }
        END {
            mean = sum / NR
            variance = (NR > 1) ? (squares - NR * mean * mean) / (NR - 1) : 0
            if (variance < 0) variance = 0
            printf "%.2f %.2f %g %g\n", mean, sqrt(variance), min, max
        }'
}

# gap <length> <reference>: relative excess of length over reference, in percent.
gap() {
    awk -v found="$1" -v reference="$2" 'BEGIN { printf "%.2f\n", 100 * (found - reference) / reference }'
}

# gap_summary <file of gaps>: prints "percentage of zero gaps, mean gap, worst gap".
gap_summary() {
    awk '{ sum += $1; if ($1 == 0) hits++; if ($1 > worst) worst = $1 }
         END { printf "%.0f %.3f %.2f", 100 * hits / NR, sum / NR, worst }' "$1"
}

log() {
    echo "[$(date +%H:%M:%S)] $*" >&2
}

# ---------------------------------------------------------------------------
# 1. The instances of the assignment: baselines against three colony variants
# ---------------------------------------------------------------------------

log "course instances"
{
    echo "# instance cities nn two_opt exact as_mean as_std as_best elitist_mean elitist_std elitist_best hybrid_mean hybrid_std hybrid_best"
    for file in "$COURSE"/tsp*.txt; do
        name=$(basename "$file" .txt)
        cities=$(head -n 1 "$file" | tr -d '[:space:]')
        nn=$(solve "$file" --algorithm nearest-neighbour)
        two_opt=$(solve "$file" --algorithm two-opt)
        exact=nan
        if ((cities <= 20)); then
            exact=$(solve "$file" --algorithm exact)
        fi
        read -r as_mean as_std as_best _ < <(repeat "$file" | stats)
        read -r el_mean el_std el_best _ < <(repeat "$file" --elitist "$cities" | stats)
        read -r hy_mean hy_std hy_best _ < <(repeat "$file" --local-search 2opt | stats)
        echo "$name $cities $nn $two_opt $exact $as_mean $as_std $as_best $el_mean $el_std $el_best $hy_mean $hy_std $hy_best"
    done
} >"$OUT/course.dat"

# The same numbers as the rows of a LaTeX table. The tiny offset makes awk
# round a mean such as 284.15 to 284.2, as a reader would, although its
# binary representation lies just below.
awk '
    function tenth(x) { return sprintf("%.1f", x + 1e-9) }
    !/^#/ {
        exact = ($5 == "nan") ? "--" : $5
        printf "\\texttt{%s} & %d & %s & %d & %d & %s & %d & %s & %d & %s & %d \\\\\n",
               $1, $2, exact, $3, $4, tenth($6), $8, tenth($9), $11, tenth($12), $14
    }' "$OUT/course.dat" >"$OUT/course_rows.tex"

# ---------------------------------------------------------------------------
# 2. Small random instances: how often is the proven optimum reached?
# ---------------------------------------------------------------------------

log "comparison with the exact solver"
{
    echo "# cities instances as_optimal_percent as_mean_gap as_worst_gap hybrid_optimal_percent hybrid_mean_gap hybrid_worst_gap"
    for cities in 8 10 12 14 16; do
        instances=100
        : >"$WORK/as.gaps"
        : >"$WORK/hybrid.gaps"
        for ((seed = 1; seed <= instances; seed++)); do
            "$ACO_TSP" generate --cities "$cities" --extent 100 --seed "$seed" \
                --output "$WORK/small.tsp" 2>/dev/null
            optimum=$(solve "$WORK/small.tsp" --algorithm exact)
            gap "$(solve "$WORK/small.tsp" --seed "$seed")" "$optimum" >>"$WORK/as.gaps"
            gap "$(solve "$WORK/small.tsp" --seed "$seed" --local-search 2opt)" "$optimum" \
                >>"$WORK/hybrid.gaps"
        done
        echo "$cities $instances $(gap_summary "$WORK/as.gaps") $(gap_summary "$WORK/hybrid.gaps")"
    done
} >"$OUT/exact.dat"

awk '!/^#/ { printf "%d & %d & %d\\,\\%% & %.2f\\,\\%% & %.1f\\,\\%% & %d\\,\\%% & %.2f\\,\\%% \\\\\n",
                    $1, $2, $3, $4, $5, $6, $7 }' "$OUT/exact.dat" >"$OUT/exact_rows.tex"

# ---------------------------------------------------------------------------
# 3. Sensitivity to alpha, beta and rho on 100 random cities
# ---------------------------------------------------------------------------

sweep() {
    local option=$1 output=$2 value
    shift 2
    {
        echo "# value mean std best worst"
        for value in "$@"; do
            echo "$value $(repeat "$UNIFORM" "--$option" "$value" | stats)"
        done
    } >"$output"
}

log "sensitivity to alpha"
sweep alpha "$OUT/sensitivity_alpha.dat" 0 0.5 1 1.5 2 3 4
log "sensitivity to beta"
sweep beta "$OUT/sensitivity_beta.dat" 0 1 2 3 4 5 6 8 10
log "sensitivity to rho"
sweep rho "$OUT/sensitivity_rho.dat" 0.02 0.05 0.1 0.2 0.3 0.5 0.7 0.9 1

# ---------------------------------------------------------------------------
# 4. Convergence of three variants, averaged over the seeds
# ---------------------------------------------------------------------------

# trace_mean <output> [options...]: mean best-so-far length at each iteration.
trace_mean() {
    local output=$1 seed
    shift
    : >"$WORK/traces.csv"
    for ((seed = 1; seed <= RUNS; seed++)); do
        solve "$UNIFORM" --iterations "$ITERATIONS" --seed "$seed" \
            --trace "$WORK/trace.csv" "$@" >/dev/null
        tail -n +2 "$WORK/trace.csv" >>"$WORK/traces.csv"
    done
    awk -F, '{ sum[$1] += $5; count[$1]++ }
             END { for (i = 1; i in sum; i++) printf "%d %.2f\n", i, sum[i] / count[i] }' \
        "$WORK/traces.csv" >"$output"
}

log "convergence"
trace_mean "$WORK/as.trace"
trace_mean "$WORK/elitist.trace" --elitist 100
trace_mean "$WORK/hybrid.trace" --local-search 2opt
{
    echo "# iteration ant_system elitist hybrid"
    paste -d' ' "$WORK/as.trace" "$WORK/elitist.trace" "$WORK/hybrid.trace" |
        awk '{ print $1, $2, $4, $6 }'
} >"$OUT/convergence.dat"

# ---------------------------------------------------------------------------
# 5. Tours to draw, and the headline numbers quoted in the text
# ---------------------------------------------------------------------------

log "tours"
uniform_nn=$(solve "$UNIFORM" --algorithm nearest-neighbour --plot "$OUT/tour_nearest_neighbour.dat")
uniform_two_opt=$(solve "$UNIFORM" --algorithm two-opt --plot "$OUT/tour_two_opt.dat")
uniform_as=$(solve "$UNIFORM" --seed 1 --plot "$OUT/tour_ant_system.dat")
uniform_hybrid=$(solve "$UNIFORM" --seed 1 --local-search 2opt --plot "$OUT/tour_hybrid.dat")

read -r as_mean _ as_best _ < <(repeat "$UNIFORM" | stats)
read -r el_mean _ el_best _ < <(repeat "$UNIFORM" --elitist 100 | stats)
read -r hy_mean _ hy_best _ < <(repeat "$UNIFORM" --local-search 2opt | stats)

{
    echo "% Generated by scripts/run-experiments.sh -- do not edit."
    echo "\\newcommand{\\UniformNearestNeighbour}{$uniform_nn}"
    echo "\\newcommand{\\UniformTwoOpt}{$uniform_two_opt}"
    echo "\\newcommand{\\UniformAntSystemTour}{$uniform_as}"
    echo "\\newcommand{\\UniformHybridTour}{$uniform_hybrid}"
    echo "\\newcommand{\\UniformAntSystemMean}{$as_mean}"
    echo "\\newcommand{\\UniformAntSystemBest}{$as_best}"
    echo "\\newcommand{\\UniformElitistMean}{$el_mean}"
    echo "\\newcommand{\\UniformElitistBest}{$el_best}"
    echo "\\newcommand{\\UniformHybridMean}{$hy_mean}"
    echo "\\newcommand{\\UniformHybridBest}{$hy_best}"
    echo "\\newcommand{\\ExperimentRuns}{$RUNS}"
    echo "\\newcommand{\\ExperimentIterations}{$ITERATIONS}"
} >"$OUT/numbers.tex"

# ---------------------------------------------------------------------------
# 6. Cost of one iteration as the instance grows (machine dependent)
# ---------------------------------------------------------------------------

log "scaling"
{
    echo "# cities iterations seconds_per_iteration"
    # Fewer iterations on larger instances keep every measurement near a second.
    for pair in 25:20000 50:4000 100:600 200:80 400:12 800:3; do
        cities=${pair%%:*}
        iterations=${pair##*:}
        "$ACO_TSP" generate --cities "$cities" --seed "$cities" --output "$WORK/scale.tsp" 2>/dev/null
        seconds=$("$ACO_TSP" solve "$WORK/scale.tsp" --seed 1 --iterations "$iterations" |
            awk '$1 == "time" { print $2 }')
        awk -v n="$cities" -v i="$iterations" -v s="$seconds" 'BEGIN { printf "%d %d %.3e\n", n, i, s / i }'
    done
} >"$OUT/scaling.dat"

log "done: results written to $OUT"
