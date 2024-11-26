#!/bin/bash

# Prompt user for file details
read -p "Enter the file name: " file_name
read -p "Enter file extension: " file_extension
read -p "Enter file size in bytes: " file_size

# Create the test_files directory if it doesn't exist
mkdir -p test_files

# Generate a file with random readable characters
< /dev/urandom tr -dc 'A-Za-z0-9!@#$%^&*()-_=+[]{}|;:,.<>?/`~' | head -c "$file_size" > test_files/$file_name.$file_extension

# Verify file creation
if [[ -f "test_files/$file_name.$file_extension" ]]; then
    echo "$file_name.$file_extension created successfully in test_files/"
else
    echo "Failed to create $file_name.$file_extension"
fi