import os
import argparse

def shift_to_top(path_to_file, to_the_top):
	new_lines = ''

	for tops in to_the_top:
		f = open(path_to_file, 'r')
		for line in f:
			if tops in line:
				new_lines = new_lines + line
				break

	f = open(path_to_file, 'r')
	for line in f:
		is_there = 0
		for tops in to_the_top:
			if tops in line:
				is_there = 1

		if is_there == 0:
			new_lines = new_lines + line

	f = open(path_to_file, 'w')
	f.write(new_lines)


parser = argparse.ArgumentParser()

parser.add_argument(
	'--rtl',
	type=str,
	required=1,
	help='Absolute path to rtl files.')

args = parser.parse_args()

RTL_PATH = ''
RTL_PATH += args.rtl + '/nlb/ '
RTL_PATH += args.rtl + '/IP/sim '
RTL_PATH += args.rtl + '/SCD_RTL/ '

print('RTL_PATH: ' + RTL_PATH)

# Path to aalsdk
aalsdk = os.environ['WORKDIR']

command = 'python ' + aalsdk + '/ase/scripts/generate_ase_environment.py ' + RTL_PATH + ' -t QUESTA'
print('Executing command: ' + command)

# subprocess.call([command])
os.chdir(aalsdk + '/ase')
os.system('pwd')
os.system(command)

vlog_to_the_top = ['ccis_if_pkg', 'ccis_if_funcs_pkg', 'ccip_if_funcs_pkg', 'cci_mpf_if_pkg']
shift_to_top('vlog_files.list', vlog_to_the_top)

vhdl_to_the_top = ['dspba_library_package.vhd', 'dspba_library.vhd']
shift_to_top('vhdl_files.list', vhdl_to_the_top)