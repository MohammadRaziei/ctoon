import argparse
import re
from pathlib import Path


def parse_vars(var_list):
    """Parse variables passed as NAME=VALUE into a dictionary."""
    out = {}
    for item in var_list:
        if "=" not in item:
            raise ValueError(f"Invalid variable: {item}")
        k, v = item.split("=", 1)
        out[k] = v
    return out


def replace_vars(text, vars_dict):
    """Replace all variable placeholders in the text."""
    for k, v in vars_dict.items():
        text = text.replace(k, v)
    return text


def main():
    parser = argparse.ArgumentParser(description="Process release template.")
    parser.add_argument("-i", "--input", required=True, help="Input file path")
    parser.add_argument("-o", "--output", help="Output file path (default: overwrite input)")
    parser.add_argument("-s", "--section", required=True, help="Section name (e.g., PYTHON_SECTION)")
    parser.add_argument("-v", "--variable", action="append", required=True, help="Variables in NAME=VALUE format")

    args = parser.parse_args()

    input_path = Path(args.input)
    output_path = Path(args.output) if args.output else input_path

    content = input_path.read_text(encoding="utf-8")
    variables = parse_vars(args.variable)

    # 1. Locate the full section block:
    # <!--SECTION_BEGIN ... SECTION_END-->
    section_re = re.compile(
        rf"<!--{args.section}_BEGIN(.*?){args.section}_END-->",
        re.DOTALL,
    )

    match = section_re.search(content)
    if not match:
        raise ValueError(f"Section {args.section} not found")

    full_section = match.group(0)
    section_body = match.group(1)

    # 2. Remove everything between:
    #    PLACEHOLDER--> ... <!--PLACEHOLDER
    # This deletes the placeholder/warning content entirely
    cleaned_body = re.sub(
        r"PLACEHOLDER-->.*?<!--PLACEHOLDER",
        "",
        section_body,
        flags=re.DOTALL,
    )

    # Trim extra whitespace
    cleaned_body = cleaned_body.strip()

    # 3. Replace variables (e.g., RELEASE_NAME, VERSION_STRING)
    cleaned_body = replace_vars(cleaned_body, variables)

    # 4. Replace the entire section (including comments) with final content
    new_content = content.replace(full_section, cleaned_body)

    # 5. Write output
    output_path.write_text(new_content, encoding="utf-8")


if __name__ == "__main__":
    main()