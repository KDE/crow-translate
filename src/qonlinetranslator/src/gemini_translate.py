import os
import json
import argparse
import google.generativeai as genai
from PIL import Image

# A simple map for language codes to names (can be expanded)
LANGUAGE_NAME_MAP = {
    "en": "English",
    "es": "Spanish",
    "fr": "French",
    "de": "German",
    "ja": "Japanese",
    "ko": "Korean",
    "zh": "Chinese",
    "it": "Italian",
    "pt": "Portuguese",
    "ru": "Russian",
    "ar": "Arabic",
    "hi": "Hindi",
}

def get_language_name(code):
    return LANGUAGE_NAME_MAP.get(code.lower(), code) # Default to code if name not found

def translate_image(api_key, image_path, target_language_code, model_name="gemini-2.0-flash-lite"):
    try:
        if not os.path.exists(image_path):
            return {"error": f"Image file not found: {image_path}"}

        genai.configure(api_key=api_key)

        img = Image.open(image_path)

        target_language_name = get_language_name(target_language_code)
        prompt = f"Translate the image to {target_language_name}"

        model = genai.GenerativeModel(model_name)

        # Check if the model supports image input.
        # This is a basic check; actual model capabilities might vary.
        # For Gemini models that support vision, the content is usually a list of parts.
        response = model.generate_content([prompt, img])

        if hasattr(response, 'text') and response.text:
            return {"translation": response.text.strip()}
        elif hasattr(response, 'parts') and response.parts:
            # If the response has parts, try to extract text from the first part
            # This can vary based on the model and SDK version
            text_parts = [part.text for part in response.parts if hasattr(part, 'text')]
            if text_parts:
                return {"translation": " ".join(text_parts).strip()}
            else:
                return {"error": "No text found in response parts."}
        else:
            # Fallback for unexpected response structure
            # Log the response for debugging if possible in a real scenario
            return {"error": "Failed to extract translation from response. The response might be empty or in an unexpected format."}

    except FileNotFoundError:
        return {"error": f"Image file not found: {image_path}"}
    except Exception as e:
        # Broad exception for API errors or other issues
        # In a production script, more specific error handling would be better
        # e.g., handling google.api_core.exceptions.GoogleAPIError
        return {"error": f"An error occurred: {str(e)}"}

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Translate an image using Gemini API.")
    parser.add_argument("image_path", help="Path to the image file.")
    parser.add_argument("target_language_code", help="Target language code (e.g., 'es' for Spanish).")

    args = parser.parse_args()

    api_key = os.environ.get("GEMINI_API_KEY")
    if not api_key:
        print(json.dumps({"error": "GEMINI_API_KEY environment variable not set."}))
    else:
        result = translate_image(api_key, args.image_path, args.target_language_code)
        print(json.dumps(result))
