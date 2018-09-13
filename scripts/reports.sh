#!/bin/bash
../cmake-build-debug/test_app2disp || exit 0
lcov -c -d ../cmake-build-debug -o app2disp.info
genhtml app2disp.info -o app2disp_result
