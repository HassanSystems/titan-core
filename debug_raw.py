import subprocess
import os
import sys

# 1. SETUP
tesseract_path = r"C:\Program Files\Tesseract-OCR\tesseract.exe"
input_image = "test.png" # We will check for .PNG too
output_base = "debug_output"

# 2. FIND IMAGE
if not os.path.exists(input_image):
    if os.path.exists("test.PNG"):
        input_image = "test.PNG"
    else:
        print("CRITICAL: Image file not found.")
        sys.exit(1)

# 3. RUN TESSERACT DIRECTLY (No Python Wrapper)
print(f">> Running Tesseract on [{input_image}]...")

cmd = [tesseract_path, input_image, output_base]

try:
    # We capture the "Standard Error" channel to see why it crashes
    result = subprocess.run(cmd, capture_output=True, text=True)
    
    print("\n--- RAW ENGINE REPORT ---")
    print(f"Exit Code: {result.returncode}")
    
    if result.returncode == 0:
        print("✅ SUCCESS! Output saved to debug_output.txt")
        # Read the result
        if os.path.exists(output_base + ".txt"):
            with open(output_base + ".txt", "r") as f:
                print("TEXT FOUND: " + f.read().strip())
    else:
        print("❌ CRASH REPORT:")
        print(result.stderr) # THIS IS WHAT WE NEED
        print("-----------------------")

except Exception as e:
    print(f"Python Execution Failed: {e}")