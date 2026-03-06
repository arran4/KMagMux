find src tests -name '*.cpp' -o -name '*.h' -o -name '*.hpp' | xargs -r clang-format -i
