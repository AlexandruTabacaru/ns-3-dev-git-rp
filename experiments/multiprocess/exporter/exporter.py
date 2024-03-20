#!/usr/bin/env python3

from pathlib import Path
import subprocess
import sys
import argparse
import shutil


def export(
    input_csv_path: Path,
    dct_path: Path,
    output_file_path: Path,
    log_file_path: str = "./log.txt",
) -> None:
    """
    Given a CSV full of aggregated data measured on NS3, writes HTML displaying the results in output_file_path.

    Assumptions:
    - input_csv_path is a path to a valid, existing CSV file
    - dct_path is a directory containing the expected docToolchain project
    - output_file_path can be written to by this script
    - Docker is installed on the host machine, the Docker daemon is running in the background, and this process is privileged to run Docker
    - The template folder is a valid [docToolchain](https://doctoolchain.org/docToolchain/v2.0.x/) project, with jinja templates

    Side effects:
    - input_csv will be opened and read as input
    - output_file_path will be created if it does not already exist
    - The docToolchain binaries in dct_path will be invoked to build the output HTML, which will then be copied to output_file_path
    - stdout and stderr from docToolchain will be logged in the file at log_file_path
    """

    # Copy csv into the docToolchain project to be incorporated in the build
    build_csv_path = dct_path / "docs" / "template" / "target.csv"
    shutil.copy(input_csv_path, build_csv_path)

    # Execute docToolchain
    try:
        with open(log_file_path, "w") as out:
            subprocess.run(
                [str(Path("bin") / "dtcw"), "generateHTML"],
                stdout=out,
                stderr=out,
                cwd=dct_path,
                check=True,
            )
    except subprocess.CalledProcessError as e:
        print(f"Publish error: {e}:  Check {log_file_path} file for error.")
        sys.exit(e.returncode)

    # Copy the HTML created by docToolchain into output_file_path
    built_html = dct_path / "build" / "html5" / "template" / "experiment-results.html"
    shutil.copy(built_html, output_file_path)


def existing_filepath(filepath: str) -> Path:
    """
    If path points to an existing filepath, returns the corresponding Path object. Else, raises a ValueError.
    """
    path = Path(filepath)
    if not path.is_file():
        raise ValueError(f"{filepath} is not a file")
    return path


def dct_project(directory: str) -> Path:
    """
    If path points to the root of a docToolchain project, returns the corresponding Path object. Else, raises a ValueError.
    """
    path = Path(directory)
    if not path.is_dir():
        raise ValueError(f"{directory} is not a directory")
    if not (path / "docToolchainConfig.groovy").is_file():
        raise ValueError(f"{directory} is not a docToolchain project")
    return path


def not_a_directory(filepath: str) -> Path:
    path = Path(filepath)
    if path.is_dir():
        raise ValueError(f"{filepath} is a directory, but a file path was expected")
    if not path.parent.is_dir():
        raise ValueError(
            f"{path.parent} does not exist, cannot export file to this destination"
        )
    return path


def main():
    """
    Entrypoint for when this file is run like a script. Provides a CLI wrapper around export.
    """
    parser = argparse.ArgumentParser(
        description="Given a CSV full of aggregated data measured on NS3, generates an HTML file displaying the results.",
    )
    parser.add_argument("input_csv", help="Path to the input CSV")
    parser.add_argument(
        "dct_path", help="Path to a directory containing a docToolchain project"
    )
    parser.add_argument(
        "output_file",
        help="Path to the output HTML file, including its name and extension",
    )
    args = parser.parse_args()

    # Validate arguments
    input_csv_path = existing_filepath(args.input_csv)
    dct_path = dct_project(args.dct_path)
    output_file_path = not_a_directory(args.output_file)

    export(input_csv_path, dct_path, output_file_path)


if __name__ == "__main__":
    main()
