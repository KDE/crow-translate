import sys
import os
import json

if __name__ == "__main__":
    # sys.argv[0] is the script name
    # sys.argv[1] is expected to be image_path (unused by mock)
    # sys.argv[2] is expected to be target_language_code (unused by mock)

    mocked_output_env = os.environ.get("MOCKED_GEMINI_OUTPUT")

    if mocked_output_env:
        # Assume the environment variable contains a valid JSON string
        print(mocked_output_env)
    else:
        # Fallback if the environment variable is not set
        error_output = {"error": "MOCKED_GEMINI_OUTPUT environment variable not set for mock script"}
        print(json.dumps(error_output))
