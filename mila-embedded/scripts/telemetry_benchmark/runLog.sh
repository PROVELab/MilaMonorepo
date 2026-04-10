#!/bin/bash

set -e

python3 -m venv venv
source venv/bin/activate
pip install contextily
pip install pandas
python3 log_analyzer.py
deactivate