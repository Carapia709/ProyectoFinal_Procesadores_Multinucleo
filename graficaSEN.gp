clear
set autoscale
#set output "senalTri_fil.eps"
set grid
set style data lines

set title "Seno Ruido"
set xlabel  "n"
set ylabel	"x(n)"
plot  "seno.dat"
unset xlabel
unset ylabel
unset title
pause -1

