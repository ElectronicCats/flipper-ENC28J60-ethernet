import qrcode
from PIL import Image

# Texto o URL que quieres codificar en el QR
data = "https://electroniccats.com/store/"

# Crear el código QR
qr = qrcode.QRCode(
    version=1,  # Controla el tamaño del QR (1 es el más pequeño)
    error_correction=qrcode.constants.ERROR_CORRECT_L,
    box_size=1,
    border=0,
)
qr.add_data(data)
qr.make(fit=True)

# Generar imagen
img = qr.make_image(fill_color="black", back_color="white")

# # Redimensionar a 20x20 píxeles
img_small = img.resize((40, 40), Image.NEAREST)  # Usar NEAREST para mantener el contraste

# Guardar o mostrar la imagen
img_small.save("qr_20x20.png")
img.show()
img_small.show()