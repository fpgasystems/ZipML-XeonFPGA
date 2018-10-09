#!/bin/bash
echo $PWD
# Copy Blue bitstream library 
# ---------------------------
echo "Restoring Blue BS lib files"
echo "*********************************************************************************************"
cp -r ../lib/blue/output_files/ .
cp -r ../lib/blue/output_files/ .
cp -r ../lib/blue/qdb_file/* .

PROJ_REV1_NAME="BDW_503_BASE_2041_seed2"
PROJ_REV2_NAME="bdw_503_pr_afu_synth"
PROJ_REV3_NAME="bdw_503_pr_afu"

echo "Revision 1 : $PROJ_REV1_NAME"
echo "Revision 2 : $PROJ_REV2_NAME"
echo "Revision 3 : $PROJ_REV3_NAME"
echo "*********************************************************************************************"

SYNTH_SUCCESS=1
FIT_SUCCESS=1
ASM_SUCCESS=1

# Synthesize PR Persona
# ---------------------
quartus_syn --read_settings_files=on $PROJ_REV1_NAME -c $PROJ_REV2_NAME
SYNTH_SUCCESS=$?

# Fit PR Persona
# --------------
if [ $SYNTH_SUCCESS -eq 0 ]
then
    quartus_cdb --read_settings_files=on $PROJ_REV1_NAME -c $PROJ_REV2_NAME --export_block "root_partition" --snapshot synthesized --file "$PROJ_REV2_NAME.qdb"
    quartus_cdb --read_settings_files=on $PROJ_REV1_NAME -c $PROJ_REV3_NAME --import_block "root_partition" --file "$PROJ_REV1_NAME.qdb"
    quartus_cdb --read_settings_files=on $PROJ_REV1_NAME -c $PROJ_REV3_NAME --import_block persona1 --file "$PROJ_REV2_NAME.qdb"
    quartus_fit --read_settings_files=on $PROJ_REV1_NAME -c $PROJ_REV3_NAME
    FIT_SUCCESS=$?
else
    echo "Persona synthesis failed"
    exit
fi

# Run Assembler 
# -------------
if [ $FIT_SUCCESS -eq 0 ]
then
    quartus_asm $PROJ_REV1_NAME -c $PROJ_REV3_NAME
    ASM_SUCCESS=$?
else
    echo "Assmebler failed"
    exit 1
fi

# Report Timing
# -------------
if [ $ASM_SUCCESS -eq 0 ]
then
    quartus_sta --do_report_timing $PROJ_REV1_NAME -c $PROJ_REV3_NAME
else
    echo "Persona compilation failed"
    exit 1
fi

# Generate output files for PR persona
# ------------------------------------
if [ $ASM_SUCCESS -eq 0 ]
then
    echo "Generating PR rbf file"
    ./generate_pr_bitstream.sh
else
    echo "Persona compilation failed"
    exit 1
fi

echo ""
echo "======================================================="
echo "BDW 503 PR AFU compilation complete"
echo "AFU rbf file located at output_files/bdw_503_pr_afu.rbf"
echo "======================================================="
echo ""
