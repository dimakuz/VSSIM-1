set terminal png
set output "results.png"
set datafile separator ','
set key autotitle columnheader
set xlabel "Request size [KB]"
set ylabel "Average response time [us]"


plot "results.csv" using 1:2 with lines lw 3, \
     "" using 1:3 with lines lw 3
