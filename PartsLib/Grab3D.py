import os
import shutil

def copy_stp_files(source_dir, destination_dir):
    # Ensure the destination directory exists
    os.makedirs(destination_dir, exist_ok=True)

    # Get a list of files already in the destination directory
    existing_files = set(os.listdir(destination_dir))

    # Walk through the source directory
    for root, _, files in os.walk(source_dir):
        for file in files:
            if file.endswith('.stp') and file not in existing_files:
                source_file_path = os.path.join(root, file)
                destination_file_path = os.path.join(destination_dir, file)

                # Copy the file to the destination directory
                shutil.copy2(source_file_path, destination_file_path)
                print(f"Copied: {source_file_path} to {destination_file_path}")

# Example usage
source_directory = 'Lib'  # Replace with your source directory
destination_directory = '3D'
copy_stp_files(source_directory, destination_directory)
