import cv2
import numpy as np
import matplotlib.pyplot as plt

def extract_polarized_bayer(image):
    """
    Extracts four polarization views from a single bayered polarization camera image.

    Args:
        image (ndarray): Input grayscale image read using OpenCV.
        bit_depth (int): Bit depth used for normalization.

    Returns:
        (i_0, i_45, i_90, i_135): Four Bayered images representing different polarization angles.
    """
    # Extract each polarization angle according to the layout:
    # 0° - (row offset 1, col offset 1)
    i_0 = image[1::2, 1::2] >> 4

    # 45° - (row offset 0, col offset 1)
    i_45 = image[0::2, 1::2] >> 4

    # 90° - (row offset 0, col offset 0)
    i_90 = image[0::2, 0::2] >> 4

    # 135° - (row offset 1, col offset 0)
    i_135 = image[1::2, 0::2] >> 4

    return i_0, i_45, i_90, i_135

def debayer_16bit(image):
    """
    Debayers a 16-bit single-channel Bayer-filtered image using OpenCV's demosaicing.

    Args:
        image (ndarray): Bayer-filtered single-channel image.

    Returns:
        color_image (ndarray): Debayered color image.
    """

    # Apply OpenCV's demosaicing function for the RG Bayer pattern (16-bit depth)
    color_image = cv2.cvtColor(image, cv2.COLOR_BAYER_RG2BGR)
    return color_image

# Load the input image using OpenCV in grayscale
input_image_path = 'build/Images/image_192900067.png'
image = cv2.imread(input_image_path, cv2.IMREAD_ANYDEPTH)

# Extract the four polarized Bayer views
i_0_bayer, i_45_bayer, i_90_bayer, i_135_bayer = extract_polarized_bayer(image)

max_pixel = max([max(x) for x in i_0_bayer])

if max_pixel == 4095:
    print("Warning: Image has over-exposed pixels")
else:
    print("Max pixel: " + str(max_pixel))

# Debayer the images
i_0_color = debayer_16bit(i_0_bayer)
i_45_color = debayer_16bit(i_45_bayer)
i_90_color = debayer_16bit(i_90_bayer)
i_135_color = debayer_16bit(i_135_bayer)

# Display the results using Matplotlib
fig, axs = plt.subplots(2, 2, figsize=(15, 15))

axs[0, 0].imshow(i_0_color >> 4)
axs[0, 0].set_title("Polarization 0°")

axs[0, 1].imshow(i_45_color >> 4)
axs[0, 1].set_title("Polarization 45°")

axs[1, 0].imshow(i_90_color >> 4)
axs[1, 0].set_title("Polarization 90°")

axs[1, 1].imshow(i_135_color >> 4)
axs[1, 1].set_title("Polarization 135°")

for ax in axs.flat:
    ax.axis('off')

plt.show()