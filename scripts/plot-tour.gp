# Draws a tour written by `aco-tsp solve --plot`.
#
#   aco-tsp solve cities.tsp --plot tour.dat
#   gnuplot -e "input='tour.dat'; output='tour.svg'" scripts/plot-tour.gp
#
# Optional variables: label (a title for the picture).

if (!exists("input") || !exists("output")) {
    print "usage: gnuplot -e \"input='tour.dat'; output='tour.svg'\" plot-tour.gp"
    exit status 2
}

set terminal svg size 640,640 enhanced background rgb "white" font "Helvetica,14"
set output output

if (exists("label")) { set title label font ",16" }

unset key
unset tics
set border linewidth 1 linecolor rgb "#9ca3af"
set size ratio -1
set offsets graph 0.04, graph 0.04, graph 0.04, graph 0.04

plot input using 1:2 with lines linewidth 2 linecolor rgb "#c2410c", \
     input using 1:2 with points pointtype 7 pointsize 0.55 linecolor rgb "#111827"
