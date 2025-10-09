from google import genai
from flask import Flask, request, jsonify

# The client gets the API key from the environment variable `GEMINI_API_KEY`.
client = genai.Client()

app = Flask(__name__)

# Store conversation history per session (or globally)
LLMInstructions = "I want you to only reply only with 'Instructions not defined'"

@app.route("/health", methods=["GET"])
def health_check():
    return jsonify({"status": "OK"})

@app.route("/set_instructions", methods=["POST"])
def set_instructions():
    global LLMInstructions
    data = request.get_json()
    LLMInstructions = data.get("instructions", "")
    print("LLM instructions updated to:", LLMInstructions)
    return jsonify({"status": "ok", "new_instructions": LLMInstructions})


@app.route("/send_to_llm", methods=["POST"])
def send_to_llm():
    data = request.get_json()
    print("Received data:", data)
    prompt = data.get("prompt", "")

    # Send prompt to Gemini
    response = client.models.generate_content(
        model="gemini-2.5-flash",
        contents=prompt,
        config=genai.types.GenerateContentConfig(
            system_instruction=LLMInstructions
        )
    )

    print("Raw response object:", response)
    print("Generated text:", response.text)

    # Return the text directly
    return jsonify({"llm_response": response.text})

if __name__ == "__main__":
    #app.run(port=5000, debug=True)
    app.run(host="0.0.0.0", port=5000, debug=True)
