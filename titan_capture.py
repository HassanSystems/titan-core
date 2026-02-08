import pyautogui
import os
try:
   screenshot = pyautogui.screenshot()
   screenshot.save('screen_memory.png')
   print('SUCCESS')
except Exception as e:
   print('FAILED: ' + str(e))
