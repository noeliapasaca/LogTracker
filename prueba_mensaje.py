import sys
from twilio.rest import Client
from dotenv import load_dotenv
import os

load_dotenv()

if len(sys.argv) != 2:
    print("Uso: python prueba_mensaje.py <numero_de_errores>")
    sys.exit(1)

numero_de_errores = int(sys.argv[1])

account_sid = os.getenv('TWILIO_ACCOUNT_SID')
auth_token = os.getenv('TWILIO_AUTH_TOKEN')
from_ = os.getenv('TWILIO_PHONE_NUMBER')
to = os.getenv('MY_PHONE_NUMBER')

client = Client(account_sid, auth_token)

mensaje = f"ALERTA: Se ha superado el umbral de errores. Total de errores: {numero_de_errores}"

message = client.messages.create(
    body=mensaje,
    from_=from_,
    to=to
)

print(f"Mensaje enviado: {mensaje}")
