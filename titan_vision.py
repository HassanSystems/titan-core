import os
import subprocess
from PIL import Image

tesseract_cmd = r'C:\Program Files\Tesseract-OCR\tesseract.exe'
original_image = r'screen_memory.png'
temp_image = 'titan_temp.bmp'

try:
    if not os.path.exists(original_image):
        if os.path.exists('screen_memory.png'): original_image = 'screen_memory.png'
        else: raise Exception('File not found')

    img = Image.open(original_image)
    img.save(temp_image, 'BMP')

    # FIX: Capture RAW BYTES (text=False) to prevent crashing on weird characters
    result = subprocess.run([tesseract_cmd, temp_image, 'stdout'], capture_output=True)
    
    # SAFELY DECODE: Ignore errors if Windows can't read a specific character
    final_text = ''
    if result.stdout:
        final_text = result.stdout.decode('utf-8', errors='ignore').strip()
    
    if not final_text: final_text = 'Image captured. No text found.'
    
    with open('titan_vision_data.txt', 'w', encoding='utf-8') as f: f.write(final_text)
    if os.path.exists(temp_image): os.remove(temp_image)

except Exception as e:
    with open('titan_vision_data.txt', 'w') as f: f.write('Error: ' + str(e))
