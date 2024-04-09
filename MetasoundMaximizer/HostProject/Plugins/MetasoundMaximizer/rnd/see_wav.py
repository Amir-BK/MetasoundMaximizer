'''
Read Wav file and plot
'''

import numpy as np
import matplotlib.pyplot as plt
import wave
from pathlib import Path as _P
import os


ROOT_FOLDER = _P(os.path.abspath(__file__)).resolve().parent.parent
# ln -s ROOT_FOLDER/googledrive_shared /Users/ishay/Library/CloudStorage/GoogleDrive-ishay@neuron.vision/My Drive/MetaSound/MaximizerTests/
# ln -s "/Users/ishay/Library/CloudStorage/GoogleDrive-ishay@neuron.vision/My Drive/MetaSound/MaximizerTests/" googledrive_shared
GOOGLE_FOLDER = ROOT_FOLDER / 'googledrive_shared'

wav1_path = GOOGLE_FOLDER / 'StereoVariant_1.wav'

from scipy.io import wavfile
samplerate, data = wavfile.read(wav1_path)
data = data[:, 0]
max_val = np.max(data, axis=0)
min_val = np.min(data, axis=0)
print(f'max_val={max_val}, min_val={min_val}')
print(f'data.shape={data.shape}, samplerate={samplerate}')

plt.plot(data, label=f"{wav1_path.name}")
plt.plot(max_val*np.ones(data.shape), label=f"max_val={max_val}")
plt.plot(min_val*np.ones(data.shape), label=f"min_val={min_val}")
plt.legend()
plt.show()

