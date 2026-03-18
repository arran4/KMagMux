import re

with open("src/core/Engine.cpp", "r") as f:
    data = f.read()

# I need to fix the logic of existing loading
# The user wants to: "add a version string, such that we load the latest version, the 'development' version is considered later if everything else is the same."
# And I broke some things and missed the `QJsonObject` include maybe?
