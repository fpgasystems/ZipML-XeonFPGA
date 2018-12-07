# ZipML on Xeon+FPGA

- Setup simulation:
$ cd glm
$ ./hw/sim/setup_ase build_sim
> In Makefile in build_sim: MENT_VSIM_OPT+= -l run.log -dpioutoftheblue 1 -novopt
> In ase.cfg change to ASE_MODE = 1 for continuous simulation