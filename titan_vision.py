import os
import subprocess
from PIL import Image

tesseract_cmd = r'C:\Program Files\Tesseract-OCR\tesseract.exe'
original_image = r'test.png'
temp_image = 'titan_temp.bmp'

try:
    # STEP 1: Find the file (Case insensitive check)
    if not os.path.exists(original_image):
        if os.path.exists('test.PNG'): original_image = 'test.PNG'
        elif os.path.exists('test.png'): original_image = 'test.png'
        else:
            raise Exception('File not found')

    # STEP 2: Sanitize (Convert to BMP)
    img = Image.open(original_image)
    img.save(temp_image, 'BMP')

    # STEP 3: Run Tesseract on the clean BMP
    # We use subprocess to call the exe directly
    result = subprocess.run([tesseract_cmd, temp_image, 'stdout'], capture_output=True, text=True)

    # STEP 4: Cleanup and Save
    final_text = result.stdout.strip()
    if not final_text: final_text = 'Image read, but no text found.'
    
    with open('titan_vision_data.txt', 'w', encoding='utf-8') as f: f.write(final_text)
    if os.path.exists(temp_image): os.remove(temp_image)

except Exception as e:
    with open('titan_vision_data.txt', 'w') as f: f.write('Error: ' + str(e))
