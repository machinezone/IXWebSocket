# Run inside the pinned suite image (Python 2) to get its exact selected case IDs.
import json
import sys
from autobahntestsuite.case import (
    Cases, CaseSetname, CaseBasename, CaseCategories, CaseSubCategories)
from autobahntestsuite.caseset import CaseSet

with open(sys.argv[1]) as source:
    spec = json.load(source)
cases = CaseSet(CaseSetname, CaseBasename, Cases, CaseCategories, CaseSubCategories)
print(json.dumps(cases.parseSpecCases(spec)))
