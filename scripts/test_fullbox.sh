#!/bin/bash

mpiexec='mpiexec'

#rm ../tmp/*.tmp

#$mpiexec -n 4 ../bin/MetroCPP ../config/test_2048.cfg
$mpiexec -n 4 ../bin/MetroCPP ../config/fullbox_1024.cfg