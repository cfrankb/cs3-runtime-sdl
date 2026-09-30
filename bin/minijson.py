import argparse
import json
import sys

def minimize_json(data):
    """
    Minimize JSON data - returns the most compact string representation,
    e.g. {"a":1,"b":[2,3]} instead of pretty-printed output.
    :param data: a Python object (dict/list/str/int/etc.) or raw JSON string
    :return: minimized JSON string
    """
    if isinstance(data, str):
        # If given a JSON string, parse it first to normalize the structure
        data = json.loads(data)
    return json.dumps(
        data,
        separators=(',', ':'),   # no space after ',' or ':' (default adds spaces!)
        ensure_ascii=False      # keep unicode chars as-is instead of \uXXXX escapes
    )

def remove_empty(obj):
    """Recursively drop keys whose value is None or an empty string."""
    if isinstance(obj, dict):
        cleaned = {}
        for k, v in obj.items():
            new_v = remove_empty(v)
            if new_v is not None and new_v != "":
                cleaned[k] = new_v
        return cleaned
    elif isinstance(obj, list):
        return [remove_empty(item) for item in obj]
    else:
        return obj

def main():
    parser = argparse.ArgumentParser(
        description="Minimize a JSON file: drops None/empty-string keys and "
                    "prints the compact representation."
    )
    parser.add_argument("input", help="path to the JSON file to minimize")
    parser.add_argument(
        "-o", "--output",
        help="write the minimized JSON to this file (defaults to stdout)"
    )
    args = parser.parse_args()

    with open(args.input, "r", encoding="utf-8") as f:
        data = remove_empty(json.load(f))

    result = minimize_json(data)
    if args.output:
        with open(args.output, "w", encoding="utf-8") as f:
            f.write(result)
        print(f"Wrote minimized JSON to {args.output}", file=sys.stderr)
    else:
        print(result)

if __name__ == "__main__":
    main()