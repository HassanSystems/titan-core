import os
import pytesseract

# 1. SETUP
pytesseract.pytesseract.tesseract_cmd = r'C:\Program Files\Tesseract-OCR\tesseract.exe'

# 2. FIND THE FILE
filename = "test.png"
if not os.path.exists(filename):
    filename = "test.PNG"

# 3. CALCULATE ABSOLUTE PATH
full_path = os.path.abspath(filename)
print(f"Checking Path: {full_path}")

# 4. CHECK IF FILE IS ACTUALLY THERE
if os.path.exists(full_path):
    print("✅ FILE EXISTS on disk.")
    try:
        # 5. ATTEMPT READ
        print(">> Asking Tesseract to read...")
        text = pytesseract.image_to_string(full_path)
        print("✅ SUCCESS! Text found:")
        print(text)
    except Exception as e:
        print(f"❌ TESSERACT FAILED: {e}")
else:
    print("❌ FILE NOT FOUND. Python cannot see it.")