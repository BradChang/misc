This is a skeleton of a multi-process "compute" worker. 

    ./compute -j 2

runs the main program with two worker processes.

The structure used by this simple program has the main (original)
process become a manager for the worker processes. Each worker 
process writes status messages to a pipe periodically. The parent
reads those pipe statuses and prints them. The parent gets EOF
on the status pipe, when all workers have finished, and closed 
their end of the pipe.

This example also memory maps an output file at startup. The idea
is that each worker would update a portion of the memory buffer.
At program termination the file backing this buffer would contain
the output.

The sample worker here just sleeps.
