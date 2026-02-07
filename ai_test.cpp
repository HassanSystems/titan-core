#include <iostream>
#include "httplib.h"
#include "json.hpp"

using namespace std;
using json = nlohmann::json;

int main() {
    cout << ">> [TEST] Connecting to AI Brain (Ollama)..." << endl;
    httplib::Client cli("http://localhost:11434");

    json request_body = {
        {"model", "llama3.2"},
        {"prompt", "Explain C++ in 5 words."},
        {"stream", false} 
    };

    auto res = cli.Post("/api/generate", request_body.dump(), "application/json");

    if (res && res->status == 200) {
        json response = json::parse(res->body);
        string answer = response["response"];
        cout << ">> [SUCCESS] AI Responded: " << endl;
        cout << answer << endl;
    } else {
        cout << ">> [ERROR] Connection Failed." << endl;
    }
    return 0;
}