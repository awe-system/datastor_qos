#!/bin/bash

lcov -c -d ../cmake-build-debug -o app.info
genhtml app.info -o cc_result