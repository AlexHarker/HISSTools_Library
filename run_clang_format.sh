#!/bin/bash

echo "Formatting..."

git ls-files "AudioFile/*.hpp" | xargs clang-format -i .
