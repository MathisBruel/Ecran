from http.server import HTTPServer, BaseHTTPRequestHandler
import urllib.parse
import requests
from urllib.parse import parse_qs
import json
import socket
import urllib3
from requests.packages.urllib3.exceptions import InsecureRequestWarning
import threading
import time

# Désactiver les avertissements pour les requêtes sans vérification SSL
urllib3.disable_warnings(InsecureRequestWarning)

# Configuration Telegram
TELEGRAM_BOT_TOKEN = "7839952728:AAGSe8eMUBPqnWYdkqsomjvzywTnrfVgagc"
TELEGRAM_CHAT_ID = "-4625122029"
TELEGRAM_API_IP = "149.154.167.220"  # IP de api.telegram.org

# Configuration Arduino
ARDUINO_IP = "192.168.1.202"
ARDUINO_PORT = 80

# Variables globales
last_update_id = 0
last_arduino_state = {"alarm": False, "movement": False}

def check_telegram_updates():
    global last_update_id
    
    while True:
        try:
            # Récupérer les mises à jour de Telegram
            telegram_url = f"https://{TELEGRAM_API_IP}/bot{TELEGRAM_BOT_TOKEN}/getUpdates"
            params = {
                "offset": last_update_id + 1,
                "timeout": 30,
                "allowed_updates": ["message"]
            }
            
            response = requests.get(
                telegram_url, 
                params=params,
                headers={"Host": "api.telegram.org"},
                verify=False
            )
            
            if response.status_code == 200:
                updates = response.json()
                if updates["ok"]:
                    for update in updates["result"]:
                        if update["update_id"] > last_update_id:
                            last_update_id = update["update_id"]
                            
                            if ("message" in update and 
                                str(update["message"]["chat"]["id"]) == TELEGRAM_CHAT_ID and 
                                "text" in update["message"]):
                                
                                message_text = update["message"]["text"].lower()
                                from_user = update["message"]["from"]["first_name"]

                                # Traiter les commandes
                                if message_text == "/on":
                                    requests.get(f"http://{ARDUINO_IP}/?alarm=on")
                                elif message_text == "/off":
                                    requests.get(f"http://{ARDUINO_IP}/?alarm=off")
                                else:
                                    # Envoyer le message à l'Arduino
                                    requests.post(
                                        f"http://{ARDUINO_IP}/update",
                                        json={"message": f"{from_user}: {message_text}"}
                                    )
        except Exception as e:
            print(f"Erreur lors de la vérification des mises à jour Telegram: {e}")
        
        time.sleep(1)

def check_arduino_status():
    global last_arduino_state
    
    while True:
        try:
            # Vérifier l'état de l'Arduino
            response = requests.get(f"http://{ARDUINO_IP}/status")
            if response.status_code == 200:
                current_state = response.json()
                
                # Vérifier les changements d'état
                if current_state != last_arduino_state:
                    changes = []
                    if current_state["alarm"] != last_arduino_state["alarm"]:
                        status = "activée" if current_state["alarm"] else "désactivée"
                        changes.append(f"🔔 Alarme {status}")
                    
                    if current_state["movement"] != last_arduino_state["movement"]:
                        if current_state["movement"]:
                            changes.append("⚠️ Mouvement détecté!")
                    
                    if changes:
                        # Envoyer les changements via Telegram
                        message = "\n".join(changes)
                        telegram_url = f"https://{TELEGRAM_API_IP}/bot{TELEGRAM_BOT_TOKEN}/sendMessage"
                        requests.post(
                            telegram_url,
                            json={"chat_id": TELEGRAM_CHAT_ID, "text": message},
                            headers={"Host": "api.telegram.org"},
                            verify=False
                        )
                    
                    last_arduino_state = current_state
        except Exception as e:
            print(f"Erreur lors de la vérification de l'état de l'Arduino: {e}")
        
        time.sleep(1)

class ProxyHandler(BaseHTTPRequestHandler):
    def log_message(self, format, *args):
        # Désactive les logs HTTP
        pass

    def do_GET(self):
        if self.path.startswith('/send'):
            # Extraire le message de l'URL
            query = urllib.parse.urlparse(self.path).query
            params = parse_qs(query)
            message = params.get('message', [''])[0]
            
            try:
                # Envoyer à Telegram via HTTPS en utilisant l'IP directe
                telegram_url = f"https://{TELEGRAM_API_IP}/bot{TELEGRAM_BOT_TOKEN}/sendMessage"
                response = requests.post(
                    telegram_url, 
                    json={"chat_id": TELEGRAM_CHAT_ID, "text": message},
                    headers={"Host": "api.telegram.org"},
                    verify=False
                )
                
                self.send_response(200)
                self.send_header('Content-type', 'text/plain')
                self.end_headers()
                if response.status_code == 200:
                    self.wfile.write(b"OK")
                else:
                    self.wfile.write(b"ERROR")
            except Exception as e:
                self.send_response(500)
                self.send_header('Content-type', 'text/plain')
                self.end_headers()
                self.wfile.write(b"ERROR")

# Démarrer les threads de surveillance
telegram_thread = threading.Thread(target=check_telegram_updates, daemon=True)
arduino_thread = threading.Thread(target=check_arduino_status, daemon=True)
telegram_thread.start()
arduino_thread.start()

print("Proxy démarré sur le port 8080")
server = HTTPServer(('0.0.0.0', 8080), ProxyHandler)
server.serve_forever()