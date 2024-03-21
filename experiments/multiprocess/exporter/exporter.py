#!/usr/bin/env python3

import io
from pathlib import Path
import subprocess
import sys
import argparse
import shutil
import jinja2
import csv


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

    # Read csv in
    with input_csv_path.open(newline="") as csv_in:
        reader = csv.DictReader(csv_in)
        data = [row for row in reader]

    # Initialize Jinja2
    templates_dir = dct_path / "jinja_templates"
    jinja_env = jinja2.Environment(
        loader=jinja2.FileSystemLoader(templates_dir), undefined=jinja2.StrictUndefined
    )
    basic_export_template = jinja_env.get_template("basic_export.adoc.jinja")

    # Convert data back into csv
    csv_str = io.StringIO()
    writer = csv.DictWriter(
        csv_str, quoting=csv.QUOTE_NONNUMERIC, fieldnames=data[0].keys()
    )
    writer.writerows(data)

    # Render jinja template into AsciiDoc file
    template_input = {"raw_csv": csv_str.getvalue()}
    document_path = dct_path / "docs" / "template" / "body.adoc"
    render_to_file(basic_export_template, document_path, template_input)

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


# Render a Jinja2 template to an output file at path
def render_to_file(template: jinja2.Template, path: Path, data: dict) -> None:
    content = template.render(data)
    with path.open("w") as output_file:
        output_file.write(content)


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
