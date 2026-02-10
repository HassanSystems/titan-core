
import os, subprocess
try:
    tesseract_cmd = r'C:\Program Files\Tesseract-OCR\tesseract.exe'
    img_path = 'screenmemory.png'
    if not os.path.exists(img_path): exit()
    result = subprocess.run([tesseract_cmd, img_path, 'stdout'], capture_output=True)
    text = result.stdout.decode('utf-8', errors='ignore').strip()
    with open('titan_vision_data.txt', 'w', encoding='utf-8') as f: f.write(text)
except: pass
