#!/usr/bin/env python3
import toml
import re
import copy
import argparse
import logging


def load_toml_template(file_path):
    """Load the TOML template file."""
    with open(file_path, "r") as file:
        return toml.load(file)


def decode_template(template, variables):
    """Replace all variable references in the template with their values using Tcl-like rules.

    If template is a dict, recursively process only the values.
    If template is a string, replace variable references.
    """
    if isinstance(template, dict):
        # For dictionaries, recursively process only the values
        return {
            key: decode_template(value, variables) for key, value in template.items()
        }
    elif isinstance(template, str):
        # For strings, perform variable replacement
        result = template
        for var_name, var_value in variables.items():
            # Escape special regex characters in variable names
            escaped_name = re.escape(var_name)
            # Match either ${var_name} or $var_name\b
            pattern = rf"\$\{{{escaped_name}\}}|\${escaped_name}\b"
            replacement = str(var_value)
            result = re.sub(pattern, replacement, result)
        return result
    else:
        # For other types, return as-is
        return template


def validate_loop_variables(loop_variables):
    """Validate that all loop variables have the same length."""
    if not loop_variables:
        return False, 0

    # Get the length of the first loop variable array
    first_var = next(iter(loop_variables.values()))
    expected_length = len(first_var)

    # Check that all other loop variables have the same length
    for var_name, values in loop_variables.items():
        if len(values) != expected_length:
            logging.error(
                f"Loop variable '{var_name}' has length {len(values)}, expected {expected_length}."
            )
            return (False, 0)

    return True, expected_length


def process_multi_loop_templates(data):
    """Process templates with multiple loop variables of the same size."""
    loop_results = []
    templates = {}
    # Extract all templates from data
    for key in data:
        if key.endswith("_templates"):
            template_type = key.split("_templates")[0]
            # Remove "_template" suffix
            templates[template_type] = data[key]

    base_variables = data.get("variables", {})
    loop_variables = data.get("loop_variables", {})

    # Validate that all loop variables have the same length
    valid, result = validate_loop_variables(loop_variables)
    if not valid:
        return [], result

    num_iterations = result

    # Process each iteration
    for i in range(num_iterations):
        # Create a copy of the base variables
        current_variables = copy.deepcopy(base_variables)

        # Add the current value of each loop variable
        iteration_values = {}
        for var_name, values in loop_variables.items():
            current_variables[var_name] = values[i]
            iteration_values[var_name] = values[i]

        # Process each template with the current variable set
        template_results = {}
        for template_type, template_dict in templates.items():
            processed = {}
            for template_name, template_text in template_dict.items():
                processed[template_name] = decode_template(
                    template_text, current_variables
                )
            template_results[template_type] = processed

        loop_results.append(
            {
                "iteration": i + 1,
                "values": iteration_values,
                "results": template_results,
            }
        )

    return loop_results, num_iterations


def process_toml(file_path: str) -> list:
    data = load_toml_template(file_path)
    loop_results, _ = process_multi_loop_templates(data)
    return loop_results


def main():
    parser = argparse.ArgumentParser(description="Decode templates from a toml file.")
    parser.add_argument("file_path", type=str, help="Path to the toml template file")
    args = parser.parse_args()
    # Replace with your actual toml file path
    file_path = args.file_path

    try:
        # Load the toml data
        data = load_toml_template(file_path)

        # Process multi-loop templates
        logging.info("MULTI-LOOP TEMPLATE PROCESSING:")
        loop_results, num_iterations = process_multi_loop_templates(data)

        if isinstance(loop_results, list) and loop_results:
            logging.info(
                f"Processing {num_iterations} iterations with multiple loop variables"
            )

            for item in loop_results:
                logging.info(f"Case {item['iteration']}:")
                logging.info("  Variables:")
                for var_name, value in item["values"].items():
                    logging.info(f"    ${var_name} = {value}")

                logging.info("  Results:")
                for name, text in item["results"].items():
                    logging.info(f"    {name.upper()}: {text}")
        else:
            logging.error(f"Error: {loop_results}")

    except FileNotFoundError:
        logging.error(f"Error: File '{file_path}' not found.")
    except Exception as e:
        logging.error(f"Error: {str(e)}")


if __name__ == "__main__":
    # Configure logging
    logging.basicConfig(
        level=logging.DEBUG, format="%(asctime)s - %(levelname)s - %(message)s"
    )

    main()
