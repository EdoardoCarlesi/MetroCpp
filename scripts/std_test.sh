#!/bin/bash

mpiexec='/home/eduardo/CLUES/libs/bin/mpiexec'

rm ../tmp/*.tmp

#$mpiexec -n 4 ../bin/MetroCPP ../config/test_2048.cfg
$mpiexec -n 4 ../bin/MetroCPP ../config/test_1024.cfg
