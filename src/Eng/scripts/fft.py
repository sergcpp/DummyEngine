import numpy as np
from PIL import Image
import matplotlib.pyplot as plt
import sys

# Load image and convert to grayscale
image = Image.open(sys.argv[1]).convert("L")
image_array = np.array(image)

# Compute 2D FFT
fft = np.fft.fft2(image_array)
fft_shifted = np.fft.fftshift(fft)  # Shift zero freq to center
magnitude_spectrum = np.abs(fft_shifted)
log_spectrum = np.log1p(magnitude_spectrum)  # log(1 + x) for visibility

# Display original and spectrum
plt.figure(figsize=(12, 6))

plt.subplot(1, 2, 1)
plt.title("Original Grayscale Image")
plt.imshow(image_array, cmap="gray")

plt.subplot(1, 2, 2)
plt.title("Log Magnitude Spectrum")
plt.imshow(log_spectrum, cmap="gray")
plt.colorbar()

plt.tight_layout()
plt.show()