#include <iostream>
#include "httplib.h"
#include "json.hpp"

using namespace std;
using json = nlohmann::json;

int main() {
    cout << ">> [TEST] Connecting to AI Brain (Ollama)..." << endl;

    // 1. Setup the Client (Localhost port 11434)
    // Note: If Ollama is running, it listens here by default.
    httplib::Client cli("http://localhost:11434");

    // 2. Prepare the JSON Payload
    // We ask the model: "Explain C++ in 5 words."
    json request_body = {
        {"model", "llama3.2"},
        {"prompt", "Explain C++ in 5 words."},
        {"stream", false} 
    };

    // 3. Send POST Request
    // We send this data to the /api/generate endpoint
    auto res = cli.Post("/api/generate", request_body.dump(), "application/json");

    // 4. Check Response
    if (res && res->status == 200) {
        // Parse the JSON result
        json response = json::parse(res->body);
        string answer = response["response"];
        
        cout << ">> [SUCCESS] AI Responded: " << endl;
        cout << "------------------------------------------------" << endl;
        cout << answer << endl;
        cout << "------------------------------------------------" << endl;
    } else {
        cout << ">> [ERROR] Could not connect to Ollama." << endl;
        if (res) cout << ">> Status Code: " << res->status << endl;
        else cout << ">> Error: Connection Refused (Is Ollama running?)" << endl;
    }

    return 0;
}