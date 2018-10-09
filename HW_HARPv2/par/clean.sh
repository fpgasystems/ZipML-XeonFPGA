#!/bin/bash
echo $PWD
echo "======================================"
echo "Removing previous compilation files..."
rm -rf *.qdb db/ qdb/ output_files/ *.qws *.sld *.json *.txt *.summary  *.qarlog

echo "Restoring blue bitstream lib files..."
cp -r ../lib/blue/output_files/ $PWD
cp -r ../lib/blue/qdb_file/* $PWD
echo "Done"
echo "======================================"
