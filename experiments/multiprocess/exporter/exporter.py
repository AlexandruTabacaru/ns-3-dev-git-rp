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
    detailed_csv_path: Path,
    focused_csv_paths: list[Path],
    dct_path: Path,
    output_file_path: Path,
    log_file_path: str | None = None,
) -> None:
    """
    Given a CSV full of aggregated data measured on NS3, writes HTML displaying the results in output_file_path.

    Assumptions:
    - detailed_csv_path and focused_csv_paths are all paths to valid, existing CSV files
    - detailed_csv_path is a csv containing a large comprehensive table of data to be rendered into the output
    - focused_csv_paths is a list of csvs containing data that will be rendered into a focused section
    - output_file_path can be written to by this script
    - Docker is installed on the host machine, the Docker daemon is running in the background, and this process is privileged to run Docker
    - The dct_path is a valid [docToolchain](https://doctoolchain.org/docToolchain/v2.0.x/) project, with jinja templates

    Side effects:
    - detailed_csv_path and focused_csv_paths will be opened and read as input
    - output_file_path will be created if it does not already exist
    - The docToolchain binaries in dct_path will be invoked to build the output HTML, which will then be copied to output_file_path
    - stdout and stderr from docToolchain will be logged to the file at log_file_path if log_file_path is a str, otherwise they will be emitted as normal
    """

    # Read csvs in
    with detailed_csv_path.open() as detailed_csv_file:
        detailed_data = detailed_csv_file.read()

    focused_data_list = []
    for path in focused_csv_paths:
        name = path.stem.replace("_", " ")
        with path.open() as focused_csv_file:
            focused_data_list.append({"name": name, "content": focused_csv_file.read()})

    # Initialize Jinja2
    templates_dir = dct_path / "jinja_templates"
    jinja_env = jinja2.Environment(
        loader=jinja2.FileSystemLoader(templates_dir), undefined=jinja2.StrictUndefined
    )
    export_template = jinja_env.get_template("split_tables.adoc.jinja")

    # Render jinja template into AsciiDoc file
    template_input = {"raw_csv": detailed_data, "focused_csvs": focused_data_list}
    document_path = dct_path / "docs" / "template" / "body.adoc"
    render_to_file(export_template, document_path, template_input)

    # Execute docToolchain
    try:
        if log_file_path is not None:
            with open(log_file_path, "w") as out:
                subprocess.run(
                    [str(Path("bin") / "dtcw"), "generateHTML"],
                    stdout=out,
                    stderr=out,
                    cwd=dct_path,
                    check=True,
                )
        else:
            subprocess.run(
                [str(Path("bin") / "dtcw"), "generateHTML"],
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
    parser.add_argument(
        "-f",
        "--focused_csv",
        action="append",
        default=[],
        help="Path to a CSV with focused data. Can be specified multiple times.",
    )
    args = parser.parse_args()

    # Validate arguments
    input_csv_path = existing_filepath(args.input_csv)
    dct_path = dct_project(args.dct_path)
    output_file_path = not_a_directory(args.output_file)
    focused_csv_paths = [Path(path) for path in args.focused_csv]

    export(input_csv_path, focused_csv_paths, dct_path, output_file_path)


if __name__ == "__main__":
    main()
