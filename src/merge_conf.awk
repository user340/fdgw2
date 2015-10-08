BEGIN {
   print "sed \\";
}

/=/ { 
	split($1, x, "="); 
	print "\t-e s/^" x[1] "=[A-Za-z0-9]*/" x[1] "=" x[2] "/ \\";
}

END {
	print "\t" MASTER "\n";
}


