#!/usr/bin/env python3
import json
import re
import copy
import argparse


def load_json_template(file_path):
    """Load the JSON template file."""
    with open(file_path, "r") as file:
        return json.load(file)


def decode_template(template, variables):
    """Replace all variable references in the template with their values."""
    result = template
    # Find all variables in the template (format: $VARIABLE)
    for var_name, var_value in variables.items():
        # Create a pattern to match the variable (case-insensitive)
        pattern = f"\\${var_name}"
        # Replace all occurrences of the variable with its value
        result = re.sub(pattern, str(var_value), result, flags=re.IGNORECASE)
    return result


def validate_loop_variables(loop_variables):
    """Validate that all loop variables have the same length."""
    if not loop_variables:
        return False, "No loop variables defined."

    # Get the length of the first loop variable array
    first_var = next(iter(loop_variables.values()))
    expected_length = len(first_var)

    # Check that all other loop variables have the same length
    for var_name, values in loop_variables.items():
        if len(values) != expected_length:
            return (
                False,
                f"Loop variable '{var_name}' has length {len(values)}, expected {expected_length}.",
            )

    return True, expected_length


def process_multi_loop_templates(data):
    """Process templates with multiple loop variables of the same size."""
    loop_results = []
    arc_templates = data.get("arc_templates", {})
    endpoint_templates = data.get("endpoint_templates", {})
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
        arc_results = {}
        for template_name, template_text in arc_templates.items():
            arc_results[template_name] = decode_template(
                template_text, current_variables
            )
        endpoint_results = {}
        for template_name, template_text in endpoint_templates.items():
            endpoint_results[template_name] = decode_template(
                template_text, current_variables
            )

        loop_results.append(
            {
                "iteration": i + 1,
                "values": iteration_values,
                "results": {"arc": arc_results, "endpoint": endpoint_results},
            }
        )

    return loop_results, num_iterations


def process_json(file_path: str):
    try:
        data = load_json_template(file_path)
        loop_results, _ = process_multi_loop_templates(data)
        return loop_results
    except FileNotFoundError:
        print(f"Error: File '{file_path}' not found.")
    except json.JSONDecodeError:
        print(f"Error: Invalid JSON format in file '{file_path}'.")
    except Exception as e:
        print(f"Error: {str(e)}")


def main():
    parser = argparse.ArgumentParser(description="Decode templates from a JSON file.")
    parser.add_argument("file_path", type=str, help="Path to the JSON template file")
    args = parser.parse_args()
    # Replace with your actual JSON file path
    file_path = args.file_path

    try:
        # Load the JSON data
        data = load_json_template(file_path)

        # Process multi-loop templates
        print("MULTI-LOOP TEMPLATE PROCESSING:")
        loop_results, num_iterations = process_multi_loop_templates(data)

        if isinstance(loop_results, list) and loop_results:
            print(
                f"Processing {num_iterations} iterations with multiple loop variables"
            )

            for item in loop_results:
                print(f"\nCase {item['iteration']}:")
                print("  Variables:")
                for var_name, value in item["values"].items():
                    print(f"    ${var_name} = {value}")

                print("  Results:")
                for name, text in item["results"].items():
                    print(f"    {name.upper()}: {text}")
        else:
            print(f"Error: {loop_results}")

    except FileNotFoundError:
        print(f"Error: File '{file_path}' not found.")
    except json.JSONDecodeError:
        print(f"Error: Invalid JSON format in file '{file_path}'.")
    except Exception as e:
        print(f"Error: {str(e)}")


if __name__ == "__main__":
    main()
