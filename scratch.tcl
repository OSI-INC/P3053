# Measure the round-trip throughput of an echo server.
set data ""
set reps 10
for {set i 0} {$i < 100000} {incr i} {append data "0"}
set len [string length $data]
LWDAQ_print $t "Length $len bytes."
set sock [socket 10.0.0.37 90]
fconfigure $sock -blocking 1 -buffering line
set start [clock milliseconds]
for {set i 0} {$i < $reps} {incr i} {
	puts $sock $data
	read $sock $len
}
close $sock
set taken [expr [clock milliseconds] - $start]
set rate [format %.3f [expr 1.0*$len*$reps/$taken/1000]]
set amount [format %.3f [expr $len*$reps/1000000.0]]
LWDAQ_print $t "Downloaded $amount Mbytes in $taken ms, $rate Mbytes/s."
