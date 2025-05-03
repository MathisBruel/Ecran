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

# Variables globales
last_update_id = 0
last_check_time = 0
TELEGRAM_CHECK_INTERVAL = 2  # Intervalle en secondes entre les requêtes Telegram

def check_telegram_updates():
    global last_update_id, last_check_time
    
    while True:
        current_time = time.time()
        
        # Vérifier si 2 secondes se sont écoulées depuis la dernière requête
        if current_time - last_check_time >= TELEGRAM_CHECK_INTERVAL:
            try:
                request_start_time = time.time()
                
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
                
                request_duration = time.time() - request_start_time
                
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
                                    
                                    # Envoyer directement la commande à l'Arduino
                                    if message_text == "/on" or message_text == "/off":
                                        command = "ON" if message_text == "/on" else "OFF"
                                        try:
                                            arduino_url = f"http://192.168.1.202/telegram_command?command={command}"
                                            response = requests.get(arduino_url)
                                            if response.status_code == 200:
                                                print(f"Commande {command} envoyée avec succès à l'Arduino")
                                            else:
                                                print(f"Erreur lors de l'envoi de la commande {command} à l'Arduino")
                                        except Exception as e:
                                            print(f"Erreur de connexion à l'Arduino: {e}")
                
                # Mettre à jour le temps de la dernière vérification
                last_check_time = current_time
                    
            except Exception as e:
                print(f"Erreur lors de la vérification des mises à jour Telegram: {e}")
        
        # Petite pause pour éviter de surcharger le CPU
        time.sleep(0.1)

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

# Démarrer le thread de surveillance
telegram_thread = threading.Thread(target=check_telegram_updates, daemon=True)
telegram_thread.start()

print("Proxy démarré sur le port 8080")
server = HTTPServer(('0.0.0.0', 8080), ProxyHandler)
server.serve_forever()