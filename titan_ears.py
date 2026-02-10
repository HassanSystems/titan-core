
import sounddevice as sd
from scipy.io.wavfile import write
import whisper
import os
import warnings
import sys
import winsound # REQUIRED FOR BEEP

# Setup
sys.stdout.reconfigure(encoding='utf-8')
warnings.filterwarnings("ignore")

FS = 44100  
SECONDS = 5 
AUDIO_FILE = "titan_input.wav"
TEXT_FILE = "titan_voice_input.txt"

def listen_and_transcribe():
    print(f">> INITIALIZING EARS...")
    
    # LOAD MODEL FIRST (So we don't wait while recording)
    model = whisper.load_model("base")
    
    # READY SIGNAL
    print(f">> LISTENING NOW (Speak after beep)...")
    winsound.Beep(1000, 500) # 1000Hz for 500ms
    
    try:
        # RECORD
        recording = sd.rec(int(SECONDS * FS), samplerate=FS, channels=1)
        sd.wait()
        write(AUDIO_FILE, FS, recording)
        print(">> PROCESSING AUDIO...")

        # TRANSCRIBE
        result = model.transcribe(AUDIO_FILE)
        text = result["text"].strip()
        
        print(f">> HEARD: {text}")
        
        with open(TEXT_FILE, "w", encoding="utf-8") as f:
            f.write(text)
            
    except Exception as e:
        print(f"Error: {e}")

if __name__ == "__main__":
    listen_and_transcribe()
