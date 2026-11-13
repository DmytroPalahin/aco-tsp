# Draws the convergence of a run recorded by `aco-tsp solve --trace`.
#
#   aco-tsp solve cities.tsp --trace trace.csv
#   gnuplot -e "input='trace.csv'; output='trace.svg'" scripts/plot-trace.gp
#
# Optional variables: label (a title for the picture).

if (!exists("input") || !exists("output")) {
    print "usage: gnuplot -e \"input='trace.csv'; output='trace.svg'\" plot-trace.gp"
    exit status 2
}

set terminal svg size 800,480 enhanced background rgb "white" font "Helvetica,14"
set output output

if (exists("label")) { set title label font ",16" }

set datafile separator ","
set xlabel "iteration"
set ylabel "tour length"
set grid linecolor rgb "#e5e7eb" linetype 1
set border 3 linecolor rgb "#6b7280"
set tics nomirror
set key top right box linecolor rgb "#e5e7eb"

# Columns: iteration, iteration_best, iteration_mean, iteration_worst, global_best.
plot input skip 1 using 1:3 with lines linewidth 1.5 linecolor rgb "#9ca3af" title "iteration mean", \
     input skip 1 using 1:2 with lines linewidth 1.5 linecolor rgb "#2563eb" title "iteration best", \
     input skip 1 using 1:5 with lines linewidth 2.5 linecolor rgb "#c2410c" title "best so far"
