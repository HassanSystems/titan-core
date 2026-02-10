
import pyautogui
try:
   screenshot = pyautogui.screenshot()
   screenshot.save('screen_memory.png')
except: pass
